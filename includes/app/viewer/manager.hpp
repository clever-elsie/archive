#ifndef MANAGER_HPP
#define MANAGER_HPP
#include <atomic>
#include <mutex>
#include <random>
#include <functional>
#include <unordered_set>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <string>
#include <string_view>

#include <ext/pb_ds/tree_policy.hpp>
#include <ext/pb_ds/assoc_container.hpp>

#include <app/viewer/Info.hpp>

namespace VIEWER{
using namespace std;

template<class key, class value, class cmp=std::less<key>>
using tree=__gnu_pbds::tree<key, value, cmp, __gnu_pbds::rb_tree_tag, __gnu_pbds::tree_order_statistics_node_update>;

struct manager{
  constexpr static uint64_t Info_page_size=12;
  constexpr static std::chrono::hours cache_update_interval{1}; // 1時間間隔
  constexpr static const char dir_cache_file[]="config/dir_cache.json";
  string base_dir;
  Info*root_dir = nullptr;
  // 可乱択二分木（順序統計木）に変更
  tree<Info*, __gnu_pbds::null_type, LeafCmp> leaf_dirs;
  mutex imtex;
  random_device rds;
  mt19937_64 R;
  unordered_set<Info*> valid_info_ptrs; // 有効ポインタ集合
  filesystem::file_time_type dir_cache_last_write_time;
  std::atomic<bool> dir_cache_dirty;
  
  // キャッシュ更新システム用
  std::atomic<bool> should_stop_cache_monitor;
  std::thread cache_monitor_thread;
  std::condition_variable cache_cv;
  std::mutex cache_mutex;
  std::atomic<bool> is_full_scanning;
  std::atomic<bool> cache_loaded_from_file;
private:
  manager():R(rds()),dir_cache_dirty(false),should_stop_cache_monitor(false),is_full_scanning(false),cache_loaded_from_file(false){}
  manager(const manager&)=delete;
  manager(manager&&)=delete;
  manager& operator=(const manager&)=delete;
  manager& operator=(manager&&)=delete;
public:
  static manager& get_instance(){
    static manager instance;
    return instance;
  }
  string rel_join(const string&dir){
    size_t start=0;
    while(dir[start]=='.'||dir[start]=='/')start++;
    return std::filesystem::path(base_dir)/string(dir.begin()+start,dir.end());
  }
  // id(数値) → Info* 変換（0 は root_dir のエイリアス）
  Info* get_info_from_id(uint64_t idv) noexcept{
    return idv==0 ? root_dir : reinterpret_cast<Info*>(idv);
  }
  bool is_valid(Info* node){
    return valid_info_ptrs.contains(node);
  }

  bool load_dir_cache(const string&cache_file);
  bool save_dir_cache(const string&cache_file);
  
  // キャッシュ更新システム
  void start_cache_monitor();
  void stop_cache_monitor();
  void cache_monitor_loop();
  void trigger_full_scan_if_needed();
  void mark_cache_dirty();
  
  // デストラクタ
  ~manager();
};
} // namespace VIEWER
#endif