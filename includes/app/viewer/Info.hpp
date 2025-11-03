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
#include <system_error>
#include <app/viewer/safe_filesystem.hpp>
#include <app/retrieve.hpp>

namespace VIEWER{
using namespace std;

struct Info : public RETRIEVE::Retrieval{
  filesystem::path path;
  set<string>tag;
  vector<Info*>dirs;
  vector<string>imgs;
  filesystem::file_time_type last_write_time;
  Info*par;
  bool is_directory;
  
  // エラー状態管理
  std::error_code last_error;
  bool has_filesystem_error = false;
  
  Info()=default;
  Info(const filesystem::path&dir,Info*par_);
  ~Info();
  bool refresh(size_t depth);
  crow::json::wvalue to_json()const;
  uint64_t id()const{
    return reinterpret_cast<uint64_t>(this);
  }
  inline bool has_only_img()const{
    return imgs.size()&&dirs.empty();
  }
  
  virtual bool match(const string&s)const override{
    return tag.contains(s) || path.string().contains(s);
  }
  
  // エラー状態の確認
  bool is_accessible() const { return !has_filesystem_error; }
  bool should_retry() const;
  private:
  constexpr static array<string,5> exts{".webp",".jpg",".jpeg",".png",".gif"};
  constexpr static array<string,6> not_img{".mp4",".mp3",".flac",".aac",".wav",".txt"};
  void reload_info();
  void reload_leaf();
  void reload_dir(size_t depth);
  bool refresh_from_parent();
  
  // エラーハンドリング
  void handle_filesystem_error(const std::error_code& ec, const std::string& operation);
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