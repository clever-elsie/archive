#ifndef INFO_HPP
#define INFO_HPP
#include <cstddef>
#include <cstdint>
#include <set>
#include <array>
#include <vector>
#include <string>
#include <filesystem>
#include <crow/json.h>

namespace VIEWER{
using namespace std;

struct Info{
  filesystem::path path;
  set<string>tag;
  vector<Info*>dirs;
  vector<string>imgs;
  filesystem::file_time_type last_write_time;
  Info*par;
  bool has_only_img;
  Info()=default;
  Info(const filesystem::path&dir,Info*par_);
  ~Info();
  bool refresh(size_t depth);
  crow::json::wvalue to_json()const;
  uint64_t id()const{
    return reinterpret_cast<uint64_t>(this);
  }
  private:
  constexpr static array<string,5> exts{".webp",".jpg",".jpeg",".png",".gif"};
  constexpr static array<string,6> not_img{".mp4",".mp3",".flac",".aac",".wav",".txt"};
  void reload_info();
  void reload_leaf();
  void reload_dir(size_t depth);
};

struct LeafCmp{
  static bool operator()(const Info* a,const Info* b)noexcept{
    if(a==b) return false;
    if(a->last_write_time!=b->last_write_time) return a->last_write_time>b->last_write_time;
    if(a->path!=b->path) return a->path<b->path;
    return a<b;
  }
};

} // namespace VIEWER
#endif