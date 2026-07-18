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
is_directory(false),has_filesystem_error(false),
video_tree_ptr(&manager::get_instance().video_tree){
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
  std::filesystem::path first_media_file;
  for (size_t i = 0; i < media_type_count(); ++i) {
    const auto& vec = media_vector(static_cast<MediaType>(i));
    if (!vec.empty()) {
      first_media_file = filesystem::path(path.path) / vec[0];
      break;
    }
  }

  if (!first_media_file.empty()) {
    auto media_time_result = SafeFS::last_write_time(first_media_file);
    if (media_time_result.success()) {
      last_write_time = media_time_result.value;
    } else {
      handle_filesystem_error(media_time_result.ec, "last_write_time (leaf)");
      dirs.clear();
      for(auto&mt:media) mt.clear();
    }
  }
  const auto& videos = media_vector(MediaType::video);
  for (const auto& vid : videos) {
    std::filesystem::path full_p = this->path.path / vid;
    auto time_result = SafeFS::last_write_time(full_p);
    std::filesystem::file_time_type last_t;
    if (time_result.success()) {
      last_t = time_result.value;
    } else {
      last_t = std::filesystem::file_time_type::clock::now();
    }
    auto vf = std::make_unique<VideoFile>(this, vid, std::filesystem::path(vid).filename().string(), last_t);
    if (video_tree_ptr) {
      video_tree_ptr->insert(vf.get());
    }
    video_files.push_back(std::move(vf));
  }

  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.insert(this);
  mgr.register_node(this);
}

Info::~Info(){
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.erase(this);
  if (video_tree_ptr) {
    for (auto& vf : video_files) {
      video_tree_ptr->erase(vf.get());
    }
  }
  mgr.unregister_node(this);
}

DirectoryType Info::directory_type() const {
  const auto& images = media_vector<MediaType::image>();
  const auto& videos = media_vector<MediaType::video>();
  const auto& audios = media_vector<MediaType::audio>();
  const auto& texts = media_vector<MediaType::text>();
  const auto& docs = media_vector<MediaType::doc>();
  
  bool has_dirs = !dirs.empty();
  bool has_images = !images.empty();
  bool has_videos = !videos.empty();
  bool has_audios = !audios.empty();
  bool has_texts = !texts.empty();
  bool has_docs = !docs.empty();
  
  if (has_dirs) {
    if (!has_images && !has_videos && !has_audios && !has_texts && !has_docs) {
      return DirectoryType::only_directories;
    }
    return DirectoryType::mixed_directory;
  }
  
  // dirs が空の場合
  if (videos.size() == 1 && images.size() <= 1 && !has_audios && !has_texts && !has_docs) {
    return DirectoryType::only_one_movie;
  }
  
  if (has_videos && !has_images && !has_audios && !has_texts && !has_docs) {
    return DirectoryType::only_movies;
  }
  
  if (has_images && !has_videos && !has_audios && !has_texts && !has_docs) {
    return DirectoryType::only_images;
  }
  
  if (has_texts && !has_images && !has_videos && !has_audios && !has_docs) {
    return DirectoryType::only_text;
  }

  if (has_docs && !has_images && !has_videos && !has_audios && !has_texts) {
    return DirectoryType::only_pdfs;
  }
  
  if (has_audios && !has_images && !has_videos && !has_texts && !has_docs) {
    return DirectoryType::only_musics;
  }
  
  return DirectoryType::mixed_directory;
}

bool Info::is_trackable() const {
  auto type = directory_type();
  return type == DirectoryType::only_images ||
         type == DirectoryType::only_movies ||
         type == DirectoryType::only_one_movie ||
         type == DirectoryType::only_text ||
         type == DirectoryType::only_pdfs ||
         type == DirectoryType::only_musics;
}

std::string Info::directory_type_to_string(DirectoryType type) {
  switch (type) {
    case DirectoryType::only_images: return "only_images";
    case DirectoryType::only_movies: return "only_movies";
    case DirectoryType::only_one_movie: return "only_one_movie";
    case DirectoryType::only_text: return "only_text";
    case DirectoryType::only_pdfs: return "only_pdfs";
    case DirectoryType::only_musics: return "only_musics";
    case DirectoryType::only_directories: return "only_directories";
    case DirectoryType::mixed_directory: return "mixed_directory";
  }
  return "mixed_directory";
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

bool VideoFile::match(const string& s) const {
  return name.contains(s) || path.contains(s) || (parent_node && parent_node->match(s));
}

uint64_t VideoFile::id() const {
  return parent_node ? parent_node->id() : 0;
}

} // namespace VIEWER
