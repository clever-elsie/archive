#include <fstream>
#include <iomanip>
#include <ios>
#include <unordered_map>
#include <filesystem>

#include <crow/json.h>

#include <app/viewer/manager.hpp>
#include <app/viewer/Info.hpp>

namespace VIEWER{
// cache_file is json file

Info* json_to_info(unordered_map<uint64_t,Info*>&id2info, const crow::json::rvalue&json){
  Info*info=new Info;
  id2info[json["id"].u()]=info;
  info->par=id2info[json["par"].u()];
  info->path=filesystem::path(json["path"].s());
  for(const auto&tag:json["tag"].lo())
    info->tag.insert(tag.s());
  for(const auto&dir:json["dirs"].lo())
    info->dirs.push_back(json_to_info(id2info,dir));
  for(const auto&img:json["imgs"].lo())
    info->imgs.push_back(img.s());
  info->has_only_img=json["has_only_img"].b();
  return info;
}

bool manager::load_dir_cache(const string&cache_file){
  if(!filesystem::exists(cache_file)||!cache_file.ends_with(".json")) {
    cache_loaded_from_file = false;
    return false;
  }
  const auto json_data=crow::json::load(cache_file);
  lock_guard<mutex> lock(imtex);
  unordered_map<uint64_t,Info*> id2info;
  if(json_data.error()) {
    // 無効なJSONファイルを削除
    filesystem::remove(cache_file);
    cache_loaded_from_file = false;
    return false;
  }
  root_dir=json_to_info(id2info,json_data);
  dir_cache_last_write_time=filesystem::last_write_time(cache_file);
  cache_loaded_from_file = true;
  return true;
}

bool manager::save_dir_cache(const string&cache_file){
  if(!cache_file.ends_with(".json")) return false;
  
  // 一時ファイルに書き込み
  string temp_file = cache_file + ".tmp";
  ofstream ofs(temp_file, ios_base::trunc);
  if(ofs.fail()) return false;
  
  lock_guard<mutex> lock(imtex);
  try {
    ofs << root_dir->to_json().dump();
    ofs.close();
    
    // 書き込み成功時のみ元ファイルを置き換え
    filesystem::rename(temp_file, cache_file);
    dir_cache_last_write_time = filesystem::last_write_time(cache_file);
    return true;
  } catch(...) {
    // エラー時は一時ファイルを削除
    filesystem::remove(temp_file);
    return false;
  }
}

// キャッシュ更新システムの実装
void manager::start_cache_monitor() {
  if (cache_monitor_thread.joinable()) return;
  
  should_stop_cache_monitor = false;
  cache_monitor_thread = std::thread(&manager::cache_monitor_loop, this);
}

void manager::stop_cache_monitor() {
  should_stop_cache_monitor = true;
  cache_cv.notify_all();
  if (cache_monitor_thread.joinable())
    cache_monitor_thread.join();
}

void manager::cache_monitor_loop() {
  while (!should_stop_cache_monitor) {
    std::unique_lock<std::mutex> lock(cache_mutex);
    
    // 1時間待機（または停止信号で中断）
    if (cache_cv.wait_for(lock, cache_update_interval, [this] { 
      return should_stop_cache_monitor.load(); 
    })) break; // 停止信号を受信
    
    // フルスキャン中でない場合のみキャッシュ更新を実行
    if (!is_full_scanning.load() && dir_cache_dirty.load()) {
      lock.unlock();
      // ユーザーアクセスがないタイミングでキャッシュ更新
      std::lock_guard<std::mutex> imtex_lock(imtex);
      if (root_dir && dir_cache_dirty.load()) {
        save_dir_cache(dir_cache_file);
        dir_cache_dirty = false;
      }
    }
  }
}

void manager::trigger_full_scan_if_needed() {
  if (cache_loaded_from_file.load() && !is_full_scanning.load()) {
    is_full_scanning = true;
    
    // バックグラウンドでフルスキャンを実行
    std::thread([this]() {
      std::lock_guard<std::mutex> lock(imtex);
      if (root_dir) {
        root_dir->refresh(998244353ul);
        mark_cache_dirty();
      }
      is_full_scanning = false;
    }).detach();
  }
}

void manager::mark_cache_dirty() {
  dir_cache_dirty = true;
}

manager::~manager() {
  stop_cache_monitor();
}
} // namespace VIEWER