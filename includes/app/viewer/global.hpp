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

// グローバル変数
inline string base_dir;
inline string rel_base;
inline Info*root_dir = nullptr;
// 可乱択二分木（順序統計木）に変更
inline tree<Info*, __gnu_pbds::null_type, LeafCmp> leaf_dirs;
inline tree<Info*, __gnu_pbds::null_type, DirCmp> dirs_tree;
inline constexpr uint64_t Info_page_size=12;
inline mutex imtex;
inline random_device rds;
inline mt19937_64 R(rds());
inline filesystem::file_time_type base_time{};
inline unordered_set<Info*> valid_info_ptrs; // 有効ポインタ集合

} // namespace VIEWER