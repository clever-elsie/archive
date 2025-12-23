#include <algorithm>
#include <app/viewer/Info.hpp>
#include <app/viewer/manager.hpp>
#include <app/viewer/safe_filesystem.hpp>
#include <ranges>
#include <system_error>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <concepts>

namespace VIEWER{

Info::Info(const filesystem::path&dir,Info*par_)
:path(dir),tag(),
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
    auto img_time_result = SafeFS::last_write_time(filesystem::path(path)/imgs[0]);
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
            << " for path " << path.string() 
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