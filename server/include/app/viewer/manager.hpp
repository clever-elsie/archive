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
#include <memory>
#include <unordered_set>
#include <filesystem>
#include <optional>

#include <ext/pb_ds/tree_policy.hpp>
#include <ext/pb_ds/assoc_container.hpp>

#include <app/viewer/Info.hpp>
namespace VIEWER{
using namespace std;

template<class key, class value, class cmp=std::less<key>>
using tree=__gnu_pbds::tree<key, value, cmp, __gnu_pbds::rb_tree_tag, __gnu_pbds::tree_order_statistics_node_update>;

enum class TreeType {
  all,
  images,
  movies,
  texts,
  pdfs,
  musics,
  size_
};

struct manager{
  constexpr static std::chrono::hours cache_update_interval{1}; // 1時間間隔
  constexpr static const char dir_cache_file[]="config/dir_cache.json";
  string base_dir;
  
  std::array<tree<Info*, __gnu_pbds::null_type, LeafCmp>, static_cast<size_t>(TreeType::size_)> trackable_trees;
  VideoTree video_tree;
  
  void register_node(Info* node);
  void unregister_node(Info* node);
  
  unordered_set<Info*> valid_info_ptrs; // 有効ポインタ集合
  unordered_set<string> public_dirs; // VIEWER_DIR からの相対パス（正規化済み、フル一致）
  unique_ptr<Info> root_dir; // 下から順にデストラクタが呼ばれるので，root_dirが先に破棄されるように下に書く
  mutex imtex;
  random_device rds;
  mt19937_64 R;
  filesystem::file_time_type dir_cache_last_write_time;
  std::atomic<bool> dir_cache_dirty;
  
  // キャッシュ更新システム用
  std::thread initial_load_thread; // 初期読み込みスレッド
  std::atomic<bool> cache_loaded_from_file;
  std::atomic<bool> initial_load_started;
private:
  manager():R(rds()),dir_cache_dirty(false),cache_loaded_from_file(false),initial_load_started(false){}
  manager(const manager&)=delete;
  manager(manager&&)=delete;
  manager& operator=(const manager&)=delete;
  manager& operator=(manager&&)=delete;
public:
  static manager& get_instance(){
    static manager instance;
    return instance;
  }
  static Info* get_root_dir(){
    return get_instance().root_dir.get();
  }
  string rel_join(const string&dir){
    size_t start=0;
    while(dir[start]=='.'||dir[start]=='/')start++;
    return std::filesystem::path(base_dir)/string(dir.begin()+start,dir.end());
  }
  // id(数値) → Info* 変換（0 は root_dir のエイリアス）
  Info* get_info_from_id(uint64_t idv) noexcept{
    return idv==0 ? root_dir.get() : reinterpret_cast<Info*>(idv);
  }
  bool is_valid(Info* node){
    return valid_info_ptrs.contains(node);
  }

  bool load_dir_cache(const string&cache_file);
  bool save_dir_cache(const string&cache_file);

  // 相対パスの正規化（絶対パスなら base_dir 配下か検証して相対化）
  // 成功: 正規化済み相対パス（rootは ""）
  // 失敗: nullopt（base_dir外、..を含む等）
  std::optional<std::string> norm_rel(std::string_view in) const;

  // 公開ディレクトリ集合（configの相対パス配列）を正規化して構築
  void set_public_dirs(const std::vector<std::string>& rel_paths);
  
  // キャッシュ更新システム
  void mark_cache_dirty();
  void start_initial_load(const std::string& base_dir);
  void shutdown(); // 明示的にバックグラウンドを停止
  
  // デストラクタ
  ~manager();
};
} // namespace VIEWER
#endif