#include <algorithm>
#include <app/viewer/Info.hpp>
#include <app/viewer/manager.hpp>
#include <app/viewer/safe_filesystem.hpp>
#include <lib/utf8.hpp>
#include <ranges>
#include <system_error>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <concepts>
#include <regex>
#include <iostream>


namespace VIEWER{

// return {{begin,begin+3},{end,end+3}}《sss》なら{{0,3},{5,8}}で，外側と内側を返す
// sizeof(《)==sizeof(》)==3
// 《》が末尾にないか，ルビ中に「ひらがな，カタカナ，アルファベット，数字」以外が含まれていればルビ無しとする
// ルビ無しはr[0]>=r[2]を意味する:rは戻り値
std::array<size_t, 4> find_ruby(const std::string&dirname){
    const size_t rbegin = dirname.rfind("《"); // 《.*》の《の先頭位置
    const size_t rend   = dirname.rfind("》"); // 《.*》の》の先頭位置
    if(rbegin >= rend || rbegin == std::string::npos || rend == std::string::npos)
        return {0,0,0,0}; // ルビは存在しない
    const size_t inner_begin = rbegin + 3; // 《.*》の中の文字列の先頭位置
    const size_t outer_end   = rend + 3;   // 《.*》の》の後ろの位置
    const char*const inner_string_begin = dirname.data() + inner_begin;
    const char*const inner_string_end   = dirname.data() + rend;
    const char* itr = inner_string_begin;
    while(itr<inner_string_end){
        uint32_t cp = utf8::decode_one(itr, inner_string_end);
        if(!utf8::isalpha(cp)
         &&!utf8::isdigit(cp)
         &&!utf8::ishiragana(cp)
         &&!utf8::iskatakana(cp))
            return {0,0,0,0}; // 無効なルビ
    }
    return {rbegin, inner_begin, rend, outer_end};
}

Path::Path(const filesystem::path&path_)
:path(path_),
dirname_without_ruby(Info::remove_suffix_ruby_and_attribute(path_.filename())),
sortkey(Info::to_key(path_.filename()))
{}

std::string Info::remove_suffix_ruby_and_attribute(const std::string&dirname){
  // ルビ削除の前に将来的には属性削除も行う
  auto[rbegin,rbegin_inner,rend_inner,rend]=find_ruby(dirname);
  if(rbegin<rend_inner)
    return std::string(dirname.begin(), dirname.begin()+rbegin);
  return dirname;
}

// ディレクトリの名前からソートキーを生成
std::string Info::to_key(const std::string&dirname){
  // 英語は全て小文字に変換
  // カタカナは全てひらがなに変換
  // 濁音，半濁音，拗音は清音に戻す
  // ファイル名の末尾に《.*》があれば読み仮名として認識
  std::string key;
  const char* it = dirname.data();
  auto end = it + dirname.size();
  while(it<end){
    uint32_t cp = utf8::decode_one(it, end);
    if(utf8::isalpha(cp))
      cp = utf8::tolower(cp);
    else{
      if(utf8::iskatakana(cp))
        cp = utf8::tohiragana(cp);
      if(utf8::ishiragana(cp))
        cp = utf8::normalize_hiragana_base(cp);
    }
    utf8::encode_one(cp, key);
  }
  // この時点でdirnameの全てがkeyにおいて文字単位で正規化されている
  // 次に《.*》が有れば，それをキーとする．
  auto[rbegin,rbegin_inner,rend_inner,rend]=find_ruby(dirname);
  if(rbegin<rend_inner)
    key=std::string(dirname.begin()+rbegin_inner, dirname.begin()+rend_inner);
  return key;
}

Info::Info(const filesystem::path&dir,Info*par_)
:path(dir),
tag(),
dirs(),media(),par(par_?:this),
is_directory(false),has_filesystem_error(false){
  {
    auto time_result = SafeFS::last_write_time(dir);
    if(!time_result.success()){
      handle_filesystem_error(time_result.ec, "last_write_time");
      return;
    }
    auto dir_result = SafeFS::is_directory(dir);
    if(!dir_result.success()){
      handle_filesystem_error(dir_result.ec, "is_directory");
      return;
    }
    last_write_time = time_result.value;
    is_directory = dir_result.value;
    if(!is_directory)return;
    reload_info();
  }
  
  // 安全なdirectory_iterator作成
  auto iter_result = SafeFS::directory_iterator(dir);
  if (!iter_result.success()) {
    handle_filesystem_error(iter_result.ec, "directory_iterator");
    return;
  }
  
  for(const auto&itr:iter_result.value){
    if(itr.is_directory()){
      auto n = make_unique<Info>(itr.path(),this);
      if(n && !n->empty())
        dirs.push_back(std::move(n));
    }else{
      // 画像 / 動画 / 音声 / テキスト / ドキュメント へ振り分け
      classify_and_push(itr.path(), media);
    }
  }
  sort();
  if(auto imgs=media_vector<MediaType::image>();!imgs.empty()) {
    auto img_time_result = SafeFS::last_write_time(filesystem::path(path.path)/imgs[0]);
    if (img_time_result.success())
      last_write_time = img_time_result.value;
    else{
      handle_filesystem_error(img_time_result.ec, "last_write_time (leaf)");
      dirs.clear();
      for(auto&mt:media) mt.clear();
      // これはディレクトリなので両方空なら呼出し元にdeleteされる
    }
  }
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.insert(this);
  if(has_only_img())
    mgr.leaf_dirs.insert(this);
}

Info::~Info(){
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.erase(this);
  if(has_only_img())
    mgr.leaf_dirs.erase(this);
}


void Info::handle_filesystem_error(const std::error_code& ec, const std::string& operation) {
  last_error = ec;
  has_filesystem_error = true;
  
  // ログ出力
  std::cerr << "Filesystem error in " << operation 
            << " for path " << path.path.string() 
            << ": " << ec.message() << std::endl;
  
  // エラー種別に応じた処理
  if (ec == std::errc::permission_denied) {
    // 権限エラー: 部分的に処理を継続
  } else if (ec == std::errc::no_such_file_or_directory) {
    // ファイル不存在: 削除処理
  } else {
    // その他のエラー: 処理を停止
  }
}

bool Info::should_retry() const {
  if (!has_filesystem_error) return false;
  
  // 一時的なエラーかどうかを判定
  return last_error == std::errc::resource_unavailable_try_again ||
         last_error == std::errc::interrupted;
}

} // namespace VIEWER
