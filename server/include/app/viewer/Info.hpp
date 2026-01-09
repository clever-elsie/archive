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
#include <system_error>
#include <ranges>
#include <algorithm>
#include <crow/json.h>
#include <app/viewer/safe_filesystem.hpp>
#include <app/retrieve.hpp>

namespace VIEWER{
using namespace std;

struct Info : public RETRIEVE::Retrieval{
  // 型
  enum class MediaType{
    image, video, audio, text, doc, size_
  };
  static constexpr std::size_t media_type_count() noexcept {
    return static_cast<std::size_t>(MediaType::size_);
  }
  using MediaArray = std::array<std::vector<std::string>, (size_t)MediaType::size_>;

  enum class SortingOrder{
    name, last_write_time
  };
  // 属性
  private:
  // ファイル名は最終的に filename《.*》.attributeのように任意個の属性とルビを末尾に付けるものとする
  filesystem::path path; // config/param.json[VIEWER_DIR]からのフルパス
  string dirname_without_ruby; // path.filename() without 《.*》
  string sortkey; // dirnameをソート用に変換したもの
  set<string>tag; // .infoファイルのタグ
  vector<unique_ptr<Info>>dirs; // 子ディレクトリのリスト
  MediaArray media; // メディアファイルのリスト
  filesystem::file_time_type last_write_time; // 最終更新時刻
  Info*par; // 親ディレクトリのポインタ
  bool is_directory; // ディレクトリかどうか
  
  // エラー状態管理
  std::error_code last_error;
  bool has_filesystem_error = false;

  public: // 構築/破棄 Info.cpp
  Info()=default;
  Info(const filesystem::path&dir,Info*par_);
  ~Info();
  public: // メンバ変数ユーティリティ
  static std::string remove_suffix_ruby_and_attribute(const std::string&dirname);
  static std::string to_key(const std::string&dirname);
  
  public: // json.cpp
  static std::unique_ptr<Info> load(const crow::json::rvalue&json);
  crow::json::wvalue to_json()const;
  private: // json.cpp
  static unique_ptr<Info> from_json(unordered_map<uint64_t, Info*>&id2info, const crow::json::rvalue&json);

  public: // 継承
  virtual bool match(const string&s)const override{
    return tag.contains(s) || path.string().contains(s);
  }

  public: // 操作 refresh.cpp
  bool refresh(size_t depth);
  private: // 操作 refresh.cpp
  void reload_info();
  void reload_leaf();
  void reload_dir(size_t depth);
  bool refresh_from_parent();

  public: // 操作 sort.cpp
  void sort();
  static void sort(std::vector<Info*>&vec, SortingOrder order=SortingOrder::name, bool descendant=false);
  private: // 操作 sort.cpp
  void sort_dirs();
  void sort_media_arrays();

  public: // 操作 manip.cpp
  static MediaType classify(const std::filesystem::path& filename)noexcept(false);
  private: // 操作 manip.cpp
  void classify_and_push(const std::filesystem::path& filename, MediaArray& to_ins);
  void classify_and_push(const std::filesystem::path& filename, MediaArray& media, MediaArray& to_ins);

  public: // メディア配列アクセス access.cpp
  std::filesystem::path locate_media(MediaType type, const std::string& filename)const;
  std::string current_thumbnail_relative_path()const; // 現在のディレクトリのmedia[0][0]
  std::vector<std::string> all_thumbnail_relative_paths()const; // 現在のディレクトリが持っている全てのディレクトリのcurrent_thumbnail_relative_path
  std::string parent_thumbnail_relative_path()const; // 現在のディレクトリの親ディレクトリのcurrent_thumbnail_relative_path
  std::vector<std::string> parent_all_thumbnail_relative_paths()const; // 現在のディレクトリの親ディレクトリが持っている全てのディレクトリのcurrent_thumbnail_relative_path
  template<MediaType type>
  std::vector<std::string> media_relative_paths(SortingOrder order=SortingOrder::name, bool descendant=false)const;
  std::vector<std::string> media_relative_paths(MediaType type, SortingOrder order=SortingOrder::name, bool descendant=false)const;
  
  template<MediaType type>
  std::vector<std::string>& media_vector(){ return media[mt_index<type>()]; }
  template<MediaType type>
  const std::vector<std::string>& media_vector()const{ return media[mt_index<type>()]; }
  std::vector<std::string>& media_vector(MediaType t){ return media[mt_index(t)]; }
  const std::vector<std::string>& media_vector(MediaType t) const{ return media[mt_index(t)]; }
  
  public: // タグ tag.cpp
  int add_tag(std::string&& tag);
  int remove_tag(const std::string& tag);
  const std::vector<std::string> normalized_tags()const;
  
  public: // ディレクトリアクセス diraccess.cpp
  std::pair<std::vector<Info*>, std::vector<Info*>>
  imgdirs_or_elsedirs(SortingOrder order=SortingOrder::name, bool descendant=false)const;
  
  public: // 状態確認 status.cpp
  uint64_t id()const{ return reinterpret_cast<uint64_t>(this); }
  filesystem::file_time_type last_write_time_value()const{ return last_write_time; }
  std::string sortkey_value()const{ return sortkey; }
  uint64_t parent_id()const{ return par->id(); }
  Info* parent()const{ return par; }
  std::string relative_path()const;
  std::filesystem::path full_path()const{ return path; }
  std::string dirname()const{ return dirname_without_ruby; }
  inline bool has_only_img()const{
    for(const auto&v:media|std::views::drop(1))
      if(!v.empty()) return false;
    if(!dirs.empty()) return false;
    return media[0].size()>0;
  }
  inline bool has_any_media() const{
    for (const auto& v : media)
      if (!v.empty()) return true;
    return false;
  }
  inline bool empty()const{
    return !has_any_media() && dirs.empty();
  }
  public: // エラー状態の確認 Info.cpp
  bool is_accessible() const { return !has_filesystem_error; }
  bool should_retry() const;
  private: // エラーハンドリング Info.cpp
  void handle_filesystem_error(const std::error_code& ec, const std::string& operation);
  
  public: // 静的メンバ変数
  // MediaType → インデックス変換
  template<MediaType type>
  static constexpr std::size_t mt_index() noexcept {
    return static_cast<std::size_t>(type);
  }
  static constexpr std::size_t mt_index(MediaType t) noexcept {
    return static_cast<std::size_t>(t);
  }
  static constexpr std::string_view mt_string(MediaType t) noexcept(false) {
    switch(t){
      case MediaType::image: return "image";
      case MediaType::video: return "video";
      case MediaType::audio: return "audio";
      case MediaType::text: return "text";
      case MediaType::doc: return "doc";
      default: throw std::invalid_argument("invalid MediaType");
    }
  }
  private:
  friend struct LeafCmp;
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