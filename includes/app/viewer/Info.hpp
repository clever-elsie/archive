#pragma once
#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>
#include <string>
#include <filesystem>

namespace VIEWER{
using namespace std;

struct Info{
  string path;
  set<string>tag;
  vector<Info*>dirs;
  vector<string>imgs;
  filesystem::file_time_type last_write_time;
  uint64_t id;
  Info*par;
  bool has_only_img;
  Info(const string&dir,Info*par_);
};

struct LeafCmp{
  static bool operator()(const Info* a,const Info* b)noexcept{
    if(a==b) return false;
    if(a->last_write_time!=b->last_write_time) return a->last_write_time>b->last_write_time;
    if(a->path!=b->path) return a->path<b->path;
    return a<b;
  }
};

struct DirCmp {
  static bool operator()(const Info* a,const Info* b)noexcept{
    if(a==b) return false;
    if(a->path!=b->path) return a->path<b->path;
    return a<b;
  }
};

} // namespace VIEWER