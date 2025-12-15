#include <fstream>
#include <iomanip>
#include <ios>
#include <unordered_map>
#include <filesystem>
#include <sstream>

#include <crow/json.h>
#include <crow/logging.h>

#include <app/viewer/manager.hpp>
#include <app/viewer/Info.hpp>

namespace VIEWER{
// cache_file is json file

unique_ptr<Info> json_to_info(unordered_map<uint64_t,Info*>&id2info, const crow::json::rvalue&json){
  static const std::array<const char*, 10> required_keys = {
    "id","par","path","tag","dirs","imgs",
    "videos","audios","texts","docs"
  };
  { // 必須フィールドが全て揃っているか検証（不足していれば不適格として例外）
    std::string missing_keys;
    for(const auto* key : required_keys){
      if(!json.has(key))
        missing_keys += std::string(key) + ", ";
    }
    if(!missing_keys.empty())
      throw std::runtime_error("invalid dir_cache.json: missing key " + missing_keys);
  }

  unique_ptr<Info> info=make_unique<Info>();
  id2info[json["id"].u()]=info.get();
  info->par=id2info[json["par"].u()];
  info->path=filesystem::path(json["path"].s());
  for(const auto&tag:json["tag"].lo())
    info->tag.insert(tag.s());
  for(const auto&dir:json["dirs"].lo())
    info->dirs.push_back(json_to_info(id2info,dir));
  static auto media_push_back = [](auto&media, const auto&json){
    for(const auto&data:json)
      media.push_back(data.s());
  };
  for(auto&[mt, key]:std::array<std::pair<Info::MediaType, const char*>, 5>{
    std::pair{Info::MediaType::image, "imgs"},
    std::pair{Info::MediaType::video, "videos"},
    std::pair{Info::MediaType::audio, "audios"},
    std::pair{Info::MediaType::text, "texts"},
    std::pair{Info::MediaType::doc, "docs"},
  })media_push_back(info->media_vector(mt),json[key].lo());

  info->is_directory=json["is_directory"].b();
  using namespace std::chrono;
  info->last_write_time=filesystem::file_time_type::clock::time_point(seconds(json["last_write_time"].i()));
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.insert(info.get());
  if(info->has_only_img())
    mgr.leaf_dirs.insert(info.get());
  return info;
}

namespace {
  // dir_cache がシンボリックリンクの場合はリンク先を解決し、
  // そうでなければそのまま返す。
  std::filesystem::path resolve_cache_target(const std::string& cache_file) {
    namespace fs = std::filesystem;
    fs::path p(cache_file);
    std::error_code ec;
    while (fs::is_symlink(p, ec)) {
      auto target = fs::read_symlink(p, ec);
      if (!ec) {
        if (target.is_relative()) target = p.parent_path() / target;
        p = target;
      }
    }
    return p;
  }
}

bool manager::load_dir_cache(const string&cache_file){
  namespace fs = std::filesystem;
  fs::path target = resolve_cache_target(cache_file);
  if(!fs::exists(target)||!target.string().ends_with(".json")) {
    cache_loaded_from_file = false;
    CROW_LOG_ERROR<<"load_dir_cache "<<target<<" not exists";
    return false;
  }
  CROW_LOG_INFO<<"load_dir_cache "<<target<<" exists";
  const auto json_data=[](const fs::path&target){
    stringstream ss;
    ss << ifstream(target).rdbuf();
    return crow::json::load(ss.str());
  }(target);
  lock_guard<mutex> lock(imtex);
  unordered_map<uint64_t,Info*> id2info;
  if(json_data.error()) {
    // 無効なJSONファイルを削除
    CROW_LOG_ERROR<<"load_dir_cache "<<target<<" invalid (parse error)";
    fs::remove(target);
    cache_loaded_from_file = false;
    return false;
  }
  try{
    root_dir=json_to_info(id2info,json_data);
  }catch(const std::exception& e){
    // スキーマ不整合などで失敗した場合もキャッシュを削除してフルロードにフォールバック
    CROW_LOG_ERROR<<"load_dir_cache "<<target<<" invalid (schema): "<<e.what();
    fs::remove(target);
    cache_loaded_from_file = false;
    return false;
  }catch(...){
    CROW_LOG_ERROR<<"load_dir_cache "<<target<<" invalid (unknown error)";
    fs::remove(target);
    cache_loaded_from_file = false;
    return false;
  }
  dir_cache_last_write_time=fs::last_write_time(target);
  cache_loaded_from_file = true;
  return true;
}

bool manager::save_dir_cache(const string&cache_file){
  namespace fs = std::filesystem;
  if(!cache_file.ends_with(".json")) return false;
  fs::path target = resolve_cache_target(cache_file);
  
  // 一時ファイルに書き込み
  string temp_file = target.string() + ".tmp";
  ofstream ofs(temp_file, ios_base::trunc);
  if(ofs.fail()) return false;
  
  lock_guard<mutex> lock(imtex);
  try {
    ofs << root_dir->to_json().dump();
    ofs.close();
    
    // 書き込み成功時のみ元ファイルを置き換え
    fs::rename(temp_file, target);
    dir_cache_last_write_time = fs::last_write_time(target);
    return true;
  } catch(...) {
    // エラー時は一時ファイルを削除
    fs::remove(temp_file);
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
      if (root_dir) root_dir->refresh(998244353ul);
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