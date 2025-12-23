#ifndef INFO_HPP
#define INFO_HPP
#include <cstddef>
#include <cstdint>
#include <set>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <filesystem>
#include <crow/json.h>
#include <system_error>
#include <app/viewer/safe_filesystem.hpp>
#include <app/retrieve.hpp>

namespace VIEWER{
using namespace std;

struct Info : public RETRIEVE::Retrieval{
  enum class MediaType{
    image, video, audio, text, doc, size_
  };
  static constexpr std::size_t media_type_count() noexcept {
    return static_cast<std::size_t>(MediaType::size_);
  }
  using MediaArray = std::array<std::vector<std::string>, (size_t)MediaType::size_>;

  filesystem::path path;
  set<string>tag;
  vector<unique_ptr<Info>>dirs;
  MediaArray media;
  filesystem::file_time_type last_write_time;
  Info*par;
  bool is_directory;
  
  // エラー状態管理
  std::error_code last_error;
  bool has_filesystem_error = false;
  
  Info()=default;
  Info(const filesystem::path&dir,Info*par_);
  static std::unique_ptr<Info> load(const crow::json::rvalue&json);
  private:
  static unique_ptr<Info> from_json(unordered_map<uint64_t, Info*>&id2info, const crow::json::rvalue&json);
  public:
  ~Info();
  bool refresh(size_t depth);
  crow::json::wvalue to_json()const;
  uint64_t id()const{
    return reinterpret_cast<uint64_t>(this);
  }
  inline bool has_only_img()const{
    return !imgs().empty() && dirs.empty();
  }
  inline bool has_any_media() const{
    for (const auto& v : media)
      if (!v.empty()) return true;
    return false;
  }
  inline bool empty()const{
    return !has_any_media() && dirs.empty();
  }
  
  virtual bool match(const string&s)const override{
    return tag.contains(s) || path.string().contains(s);
  }
  
  // エラー状態の確認
  bool is_accessible() const { return !has_filesystem_error; }
  bool should_retry() const;
  // MediaType → インデックス変換
  static constexpr std::size_t mt_index(MediaType t) noexcept {
    return static_cast<std::size_t>(t);
  }
  // メディア配列アクセス
  inline std::vector<std::string>& media_vector(MediaType t){
    return media[mt_index(t)];
  }
  inline const std::vector<std::string>& media_vector(MediaType t) const{
    return media[mt_index(t)];
  }
  // 便宜用アクセッサ
  inline std::vector<std::string>& imgs(){ return media_vector(MediaType::image); }
  inline const std::vector<std::string>& imgs() const{ return media_vector(MediaType::image); }
  inline std::vector<std::string>& videos(){ return media_vector(MediaType::video); }
  inline const std::vector<std::string>& videos() const{ return media_vector(MediaType::video); }
  inline std::vector<std::string>& audios(){ return media_vector(MediaType::audio); }
  inline const std::vector<std::string>& audios() const{ return media_vector(MediaType::audio); }
  inline std::vector<std::string>& texts(){ return media_vector(MediaType::text); }
  inline const std::vector<std::string>& texts() const{ return media_vector(MediaType::text); }
  inline std::vector<std::string>& docs(){ return media_vector(MediaType::doc); }
  inline const std::vector<std::string>& docs() const{ return media_vector(MediaType::doc); }
  void sort_dirs();
  void sort_media_arrays();
  private:
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