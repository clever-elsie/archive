#pragma once
#include <mutex>
#include <random>
#include <functional>
#include <unordered_set>

#include <ext/pb_ds/tree_policy.hpp>
#include <ext/pb_ds/assoc_container.hpp>

#include "Info.hpp"

namespace VIEWER{
using namespace std;

template<class key, class value, class cmp=std::less<key>>
using tree=__gnu_pbds::tree<key, value, cmp, __gnu_pbds::rb_tree_tag, __gnu_pbds::tree_order_statistics_node_update>;

struct manager{
  constexpr static uint64_t Info_page_size=12;
  string base_dir;
  Info*root_dir = nullptr;
  // 可乱択二分木（順序統計木）に変更
  tree<Info*, __gnu_pbds::null_type, LeafCmp> leaf_dirs;
  tree<Info*, __gnu_pbds::null_type, DirCmp> dirs_tree;
  mutex imtex;
  random_device rds;
  mt19937_64 R;
  filesystem::file_time_type base_time{};
  unordered_set<Info*> valid_info_ptrs; // 有効ポインタ集合
private:
  manager():R(rds()){}
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
};
} // namespace VIEWER