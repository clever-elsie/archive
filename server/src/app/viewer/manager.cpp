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
#include <app/viewer/loader.hpp>

namespace VIEWER{

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
    root_dir=Info::load(json_data);
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
    dir_cache_dirty = false;
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
    
    // 1時間待機（または停止信号や汚染マーク/スキャン完了通知で中断）
    cache_cv.wait_for(lock, cache_update_interval, [this] { 
      return should_stop_cache_monitor.load() || dir_cache_dirty.load(); 
    });
    if (should_stop_cache_monitor.load()) break; // 停止信号を受信
    
    // フルスキャン中でない場合のみキャッシュ更新を実行
    if (!is_full_scanning.load() && dir_cache_dirty.load()) {
      lock.unlock();
      // ユーザーアクセスがないタイミングでキャッシュ更新
      if (root_dir && dir_cache_dirty.load()) {
        save_dir_cache(dir_cache_file);
      }
    }
  }
}

std::optional<std::string> manager::norm_rel(std::string_view in) const {
  namespace fs = std::filesystem;
  fs::path p = fs::path(in).lexically_normal();
  if (p.empty() || p == ".") return std::string(); // root
  if (!p.is_absolute()) return p.generic_string();

  fs::path base = fs::path(base_dir).lexically_normal();
  if (base.empty())[[unlikely]] return std::nullopt;

  std::string abs_s = p.generic_string();
  std::string base_s = base.generic_string();
  if (base_s.empty()) return std::nullopt;
  // prefix判定は "/" 境界を考慮（/a/b と /a/bc の誤一致を防ぐ）
  if (base_s.back()!='/') base_s.push_back('/');
  if (abs_s.back()!='/') abs_s.push_back('/');
  if (!(std::string_view(abs_s).starts_with(base_s)))
    return std::nullopt;

  fs::path rel = fs::relative(p, base).lexically_normal();
  if (rel.empty() || rel == ".") [[unlikely]] return std::string(); // root
  return rel.generic_string();
}

void manager::set_public_dirs(const std::vector<std::string>& rel_paths) {
  std::lock_guard<std::mutex> lock(imtex);
  public_dirs.clear();
  public_dirs.reserve(rel_paths.size()+1);
  public_dirs.insert(""); // root
  for (const auto& p : rel_paths) {
    if (auto n = norm_rel(p);n)
      public_dirs.insert(*n);
  }
}

void manager::trigger_full_scan_if_needed() {
  if (cache_loaded_from_file.load() && !is_full_scanning.load()) {
    is_full_scanning = true;
    
    if (full_scan_thread.joinable()) full_scan_thread.join();
    full_scan_thread = std::thread([this]() {
      std::lock_guard<std::mutex> lock(imtex);
      if (root_dir) root_dir->refresh(998244353ul);
      is_full_scanning = false;
      cache_cv.notify_all();
    });
  }
}
 
void manager::mark_cache_dirty() {
  dir_cache_dirty = true;
  cache_cv.notify_all();
}

void manager::start_initial_load(const std::string& base_dir){
  if(initial_load_started.exchange(true)) return;
  if (initial_load_thread.joinable())
    initial_load_thread.join();
  initial_load_thread = std::thread([base_dir](){
    load_leaf_dir(base_dir);
  });
}

void manager::shutdown(){
  stop_cache_monitor();
  if (full_scan_thread.joinable())
    full_scan_thread.join();
  if (initial_load_thread.joinable())
    initial_load_thread.join();
}

void manager::register_node(Info* node) {
  if (!node->is_trackable()) return;
  trackable_trees[static_cast<size_t>(TreeType::all)].insert(node);
  
  auto type = node->directory_type();
  if (type == DirectoryType::only_images) {
    trackable_trees[static_cast<size_t>(TreeType::images)].insert(node);
  } else if (type == DirectoryType::only_movies || type == DirectoryType::only_one_movie) {
    trackable_trees[static_cast<size_t>(TreeType::movies)].insert(node);
  } else if (type == DirectoryType::only_text) {
    trackable_trees[static_cast<size_t>(TreeType::texts)].insert(node);
  } else if (type == DirectoryType::only_pdfs) {
    trackable_trees[static_cast<size_t>(TreeType::pdfs)].insert(node);
  } else if (type == DirectoryType::only_musics) {
    trackable_trees[static_cast<size_t>(TreeType::musics)].insert(node);
  }
}

void manager::unregister_node(Info* node) {
  trackable_trees[static_cast<size_t>(TreeType::all)].erase(node);
  
  auto type = node->directory_type();
  if (type == DirectoryType::only_images) {
    trackable_trees[static_cast<size_t>(TreeType::images)].erase(node);
  } else if (type == DirectoryType::only_movies || type == DirectoryType::only_one_movie) {
    trackable_trees[static_cast<size_t>(TreeType::movies)].erase(node);
  } else if (type == DirectoryType::only_text) {
    trackable_trees[static_cast<size_t>(TreeType::texts)].erase(node);
  } else if (type == DirectoryType::only_pdfs) {
    trackable_trees[static_cast<size_t>(TreeType::pdfs)].erase(node);
  } else if (type == DirectoryType::only_musics) {
    trackable_trees[static_cast<size_t>(TreeType::musics)].erase(node);
  }
}

manager::~manager() {
  shutdown();
}
} // namespace VIEWER