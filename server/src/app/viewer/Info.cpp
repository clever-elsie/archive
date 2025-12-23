#include <algorithm>
#include <app/viewer/Info.hpp>
#include <app/viewer/manager.hpp>
#include <app/viewer/safe_filesystem.hpp>
#include <ranges>
#include <system_error>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <concepts>

namespace VIEWER{

namespace {

  using MT = Info::MediaType;
  constexpr std::size_t MediaTypeCount = Info::media_type_count();

  struct MediaExtConfig {
    std::array<std::string_view, 5> exts{};
    std::size_t count{};
  };

  // MediaType ごとの拡張子定義
  constexpr static std::array<MediaExtConfig, MediaTypeCount> make_media_exts(){
    return std::array<MediaExtConfig, MediaTypeCount>{{
      // image
      MediaExtConfig{{".webp",".jpg",".jpeg",".png",".gif"}, 5},
      // video
      MediaExtConfig{{".mp4","","","",""}, 1},
      // audio
      MediaExtConfig{{".mp3",".flac",".aac",".wav",""}, 4},
      // text
      MediaExtConfig{{".txt",".md","","",""}, 2},
      // doc
      MediaExtConfig{{".pdf","","","",""}, 1}
    }};
  }
  constexpr inline auto MEDIA_EXTS=make_media_exts();
  
  template<class T, class... Args>
  concept included_in = (std::is_same_v<T, Args> || ...);
  template<class T>
  concept like_string = included_in<std::decay_t<T>, std::string, std::string_view, char*, const char*>;

  namespace {
    struct TransparentHash{
      using is_transparent = void;
      template<like_string T>
      size_t operator()(const T&s)const noexcept{
        return std::hash<T>()(s);
      }
    };
    struct TransparentEqual{
      using is_transparent = void;
      bool operator()(const char* lhs, const char* rhs) const noexcept{
        for(;*lhs && *rhs; ++lhs, ++rhs)
          if(*lhs!=*rhs) return false;
        return true;
      }
      template<like_string T>
      bool operator()(const T& lhs, const T& rhs) const noexcept{
        return lhs == rhs;
      }
    };
    template<class T>
    using reverse_map_t = std::unordered_map<std::string, T, TransparentHash, TransparentEqual>;

    reverse_map_t<MT> make_reverse_media_type(){
      const auto& media_exts = make_media_exts();
      reverse_map_t<MT> ret;
      for(std::size_t i=0;i<MediaTypeCount;++i){
        auto mt = static_cast<MT>(i);
        const auto& cfg = media_exts[i];
        for(std::size_t j=0;j<cfg.count;++j)
          ret[std::string(cfg.exts[j])] = mt;
      }
      return ret;
    }
  }
  template<class T>
  MT reverse_media_type(const T&s) noexcept(false) {
    static const auto reverse_media_type_map = make_reverse_media_type();
    auto itr = reverse_media_type_map.find(s);
    if(itr==reverse_media_type_map.end())
      throw std::invalid_argument("invalid media type: " + s);
    return itr->second;
  }


  inline const MediaExtConfig& exts_for(MT t){
    return MEDIA_EXTS[Info::mt_index(t)];
  }

  // C++26ではリフレクションでこれを不要に
  inline const char* media_name(MT t){
    switch(t){
      case MT::image: return "image";
      case MT::video: return "video";
      case MT::audio: return "audio";
      case MT::text:  return "text";
      case MT::doc:   return "doc";
      default:        return "unknown";
    }
  }

  // 汎用: 拡張子に応じて MediaArray へ振り分け
  template <class OptionalMediaArray,
    class MediaArray_t=std::conditional_t<
      std::same_as<OptionalMediaArray, std::nullptr_t>,
        std::nullptr_t,
        Info::MediaArray const *
    >
  >
  requires included_in<std::remove_cv_t<OptionalMediaArray>, std::nullptr_t, Info::MediaArray * >
  void classify_and_push(const std::filesystem::path& filename, MediaArray_t media, Info::MediaArray& to_ins){
    size_t mt;
    try{
      mt=static_cast<size_t>(reverse_media_type(filename.extension().string()));
    }catch(const std::invalid_argument&){
      return;
    }
    if constexpr(!std::same_as<MediaArray_t, std::nullptr_t>){
      if(std::lower_bound((*media)[mt].begin(),(*media)[mt].end(), filename.filename().string())!=(*media)[mt].end())
        return; // 既に存在するときは何もしない
    }
    to_ins[mt].emplace_back(filename.filename().string());
  }
}

Info::Info(const filesystem::path&dir,Info*par_)
:path(dir),tag(),
dirs(),media(),par(par_?:this),
is_directory(false),has_filesystem_error(false){
  {
    auto time_result = SafeFS::last_write_time(dir);
    if(!time_result.success()){
      handle_filesystem_error(time_result.ec, "last_write_time");
      return;
    }
    auto dir_result = SafeFS::is_directory(dir);
    if(!dir_result.success()){
      handle_filesystem_error(dir_result.ec, "is_directory");
      return;
    }
    last_write_time = time_result.value;
    is_directory = dir_result.value;
    if(!is_directory)return;
    reload_info();
  }
  
  // 安全なdirectory_iterator作成
  auto iter_result = SafeFS::directory_iterator(dir);
  if (!iter_result.success()) {
    handle_filesystem_error(iter_result.ec, "directory_iterator");
    return;
  }
  
  for(const auto&itr:iter_result.value){
    if(itr.is_directory()){
      auto n = make_unique<Info>(itr.path(),this);
      if(n && !n->empty())
        dirs.push_back(std::move(n));
    }else{
      // 画像 / 動画 / 音声 / テキスト / ドキュメント へ振り分け
      classify_and_push<std::nullptr_t>(itr.path(), nullptr, media);
    }
  }
  sort_dirs();
  sort_media_arrays();
  if(!imgs().empty()) {
    auto img_time_result = SafeFS::last_write_time(filesystem::path(path)/imgs()[0]);
    if (img_time_result.success())
      last_write_time = img_time_result.value;
    else{
      handle_filesystem_error(img_time_result.ec, "last_write_time (leaf)");
      dirs.clear();
      for(auto&mt:media) mt.clear();
      // これはディレクトリなので両方空なら呼出し元にdeleteされる
    }
  }
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.insert(this);
  if(has_only_img())
    mgr.leaf_dirs.insert(this);
}

unique_ptr<Info> Info::load(const crow::json::rvalue&json){
  std::unordered_map<uint64_t, Info*> id2info;
  return from_json(id2info, json);
}

unique_ptr<Info> Info::from_json(unordered_map<uint64_t, Info*>&id2info, const crow::json::rvalue&json){
  static constexpr std::array<const char*, 10> required_keys{{
    "id","par","path","tag","dirs","imgs",
    "videos","audios","texts","docs"
  }};
  { // 必須フィールドが全て揃っているか検証（不足していれば不適格として例外）
    std::string missing_keys;
    for(const auto* key : required_keys){
      if(!json.has(key))
        missing_keys += std::string(key) + ", ";
    }
    if(!missing_keys.empty())
      throw std::runtime_error("invalid dir_cache.json: missing key " + missing_keys);
  }

  unique_ptr<Info> info=make_unique<Info>();
  id2info[json["id"].u()]=info.get();
  info->par=id2info[json["par"].u()];
  info->path=filesystem::path(json["path"].s());
  for(const auto&tag:json["tag"].lo())
    info->tag.insert(tag.s());
  for(const auto&dir:json["dirs"].lo())
    info->dirs.push_back(from_json(id2info,dir));
  static auto media_push_back = [](auto&media, const auto&json){
    for(const auto&data:json)
      media.push_back(data.s());
  };
  for(auto&[mt, key]:std::array<std::pair<Info::MediaType, const char*>, 5>{
    std::pair{Info::MediaType::image, "imgs"},
    std::pair{Info::MediaType::video, "videos"},
    std::pair{Info::MediaType::audio, "audios"},
    std::pair{Info::MediaType::text, "texts"},
    std::pair{Info::MediaType::doc, "docs"},
  })media_push_back(info->media_vector(mt),json[key].lo());

  info->is_directory=json["is_directory"].b();
  using namespace std::chrono;
  info->last_write_time=filesystem::file_time_type::clock::time_point(seconds(json["last_write_time"].i()));
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.insert(info.get());
  if(info->has_only_img())
    mgr.leaf_dirs.insert(info.get());
  info->sort_dirs();
  info->sort_media_arrays();
  return info;
}

Info::~Info(){
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.erase(this);
  if(has_only_img())
    mgr.leaf_dirs.erase(this);
}

void Info::sort_dirs(){
  std::ranges::sort(dirs,[](const std::unique_ptr<Info>&a,const std::unique_ptr<Info>&b){
    if(a->is_directory==b->is_directory)
      return a->path<b->path;
    return a->is_directory;
  });
}

void Info::sort_media_arrays(){
  for(auto& v : this->media){
    std::ranges::sort(v);
    v.erase(std::unique(v.begin(),v.end()),v.end());
  }
}

bool Info::refresh_from_parent(){
  if(par!=this) return par->refresh(0);
  else{
    auto& mgr = manager::get_instance();
    mgr.root_dir.release();
    mgr.mark_cache_dirty();
    return false;
  }
}

bool Info::refresh(size_t depth){
  // この関数は常にmanager.imtexをlockした関数から呼び出す．
  // depth==0なら再帰を止める．
  // ただし，新しく観測したディレクトリはコンストラクタを呼び出すので全再帰
  manager& mgr = manager::get_instance();
  bool has_update = false;
  
  // 安全なexists確認
  auto exists_result = SafeFS::exists(path);
  if (!exists_result.success()) {
    handle_filesystem_error(exists_result.ec, "exists");
    return false;
  }
  if(!exists_result.value){
    // 消えてるとき，親からも消さないといけないので自分の親でrefreshする
    // 親側からしか安全に操作できないときは常に親が操作することを契約する
    refresh_from_parent();
    return false;
  }
  if(has_only_img()){ // 葉ノード（画像のみ）
    auto time_result = SafeFS::last_write_time(path/imgs()[0]);
    if(!time_result.success()){
      handle_filesystem_error(time_result.ec, "last_write_time (leaf)");
      refresh_from_parent();
      return false;
    }
    if(time_result.value > this->last_write_time){ // 更新有り
      reload_info(), reload_leaf();
      this->last_write_time = time_result.value;
      has_update = true;
    }
  }else if(is_directory){ // ディレクトリ
    auto time_result = SafeFS::last_write_time(path);
    if(!time_result.success()){
      handle_filesystem_error(time_result.ec, "last_write_time (directory)");
      // 自分自身の情報を取得できないとき，他のタスクがディレクトリをロックしている
      // したがって，アクセスできないので，親からrefreshして削除してもらう
      refresh_from_parent();
      return false;
    }
    if(time_result.value > this->last_write_time){ // 更新有り
      reload_dir(depth), reload_leaf();
      this->last_write_time = time_result.value;
      has_update = true;
    }
    if(depth) // 再帰的に更新
      for(auto&dir:dirs)
        has_update |= dir->refresh(depth-1);
  }else{ // 動画・音声・テキストファイル は更新があってもやることなし
  }
  
  // 更新があった場合はキャッシュをdirtyにマーク
  if(has_update) mgr.mark_cache_dirty();
  
  return true;
}

void Info::reload_info(){
  const filesystem::path info=this->path / ".info";
  auto exists_result = SafeFS::exists(info);
  if(!exists_result.success() || !exists_result.value) {
    if(!exists_result.success())
      handle_filesystem_error(exists_result.ec, "reload_info exists");
    return;
  }
  ifstream ifs(info);
  if(ifs.fail()){
    std::error_code ec(ifs.rdstate(), std::iostream_category());
    handle_filesystem_error(ec, "reload_info .info");
    return;
  }
  string buf;
  decltype(tag) new_tag;
  while(getline(ifs,buf)){
    if(buf.size()<1)continue;
    if(buf.back()=='\n')buf.pop_back();
    new_tag.emplace(buf);
  }
  tag=std::move(new_tag);
}

void Info::reload_leaf(){
  auto iter_result = SafeFS::directory_iterator(this->path);
  if(!iter_result.success()){
    handle_filesystem_error(iter_result.ec, "reload_leaf directory_iterator");
    return;
  }
  Info::MediaArray new_media;
  for(const auto&itr:iter_result.value)
    classify_and_push<std::nullptr_t>(itr.path(), nullptr, new_media);
  media = std::move(new_media);
  sort_media_arrays();
}

void Info::reload_dir(size_t depth){
  vector<size_t> to_del;
  for(size_t i=0;i<dirs.size();){
    auto exists_result = SafeFS::exists(dirs[i]->path);
    if (!exists_result.success()) {
      handle_filesystem_error(exists_result.ec, "reload_dir exists");
      continue;
    }
    if(!exists_result.value){
      std::swap(dirs[i],dirs.back());
      dirs.pop_back();
      continue;
    }else if(depth) dirs[i]->refresh(depth-1);
    ++i;
  }// 削除済みディレクトリを削除
  // 画像・各種メディアファイルの削除チェック
  auto erase_if_missing = [this](std::vector<std::string>& files, const char* op_name){
    for(size_t i=0;i<files.size();){
      auto exists_result = SafeFS::exists(filesystem::path(this->path)/files[i]);
      if(!exists_result.success()){
        handle_filesystem_error(exists_result.ec, op_name);
        continue;
      }
      if(!exists_result.value){
        std::swap(files[i],files.back());
        files.pop_back();
        continue;
      }
      ++i;
    }
  };
  for(std::size_t i=0;i<MediaTypeCount;++i){
    auto mt = static_cast<MT>(i);
    erase_if_missing(media[Info::mt_index(mt)], media_name(mt));
  }
  sort_dirs();
  sort_media_arrays();
  vector<unique_ptr<Info>> to_ins;
  Info::MediaArray to_ins_media{};
  auto iter_result = SafeFS::directory_iterator(path);
  if(!iter_result.success()){
    handle_filesystem_error(iter_result.ec, "reload_dir directory_iterator");
    return;
  }
  for(const auto&itr:iter_result.value){
    filesystem::path p(itr.path());
    auto is_dir_result = SafeFS::is_directory(p);
    if(!is_dir_result.success()){
      handle_filesystem_error(is_dir_result.ec, "reload_dir is_directory");
      continue;
    }
    // ディレクトリ
    if(is_dir_result.value){
      using ppt = pair<filesystem::path, bool>;
      ppt pp(p, is_dir_result.value);
      auto itr_pos=std::lower_bound(dirs.begin(),dirs.end(),pp,[](const std::unique_ptr<Info>&a, const ppt&b){
        if(a->is_directory==b.second) return a->path<b.first;
        return a->is_directory;
      });
      if(itr_pos==dirs.end()||(*itr_pos)->path!=p){// 新規
        auto n = make_unique<Info>(p, this);
        if(n && !n->empty())
          to_ins.push_back(std::move(n));
      }
    }else{
      // 画像 / 動画 / 音声 / テキスト / ドキュメント へ振り分け（差分用）
      classify_and_push<Info::MediaArray *>(itr.path(), &this->media, to_ins_media);
    }
  }
  dirs.insert(dirs.end(),std::make_move_iterator(to_ins.begin()),std::make_move_iterator(to_ins.end()));
  sort_dirs();
  for(std::size_t i=0;i<MediaTypeCount;++i){
    auto& dest = media[i];
    auto& src  = to_ins_media[i];
    dest.insert(dest.end(), src.begin(), src.end());
  }
  sort_media_arrays();
}

crow::json::wvalue Info::to_json()const{
  crow::json::wvalue json;
  crow::json::wvalue::list dirs_json;
  json["id"]=id();
  json["par"]=par->id();
  json["path"]=path.string();
  json["tag"]=crow::json::wvalue::list(tag.begin(),tag.end());
  for(const auto&dir:dirs)
    dirs_json.push_back(dir->to_json());
  json["dirs"]=crow::json::wvalue(dirs_json);
  for(auto&[mt, key]:std::array<std::pair<Info::MediaType, const char*>, 5>{
    std::pair{Info::MediaType::image, "imgs"},
    std::pair{Info::MediaType::video, "videos"},
    std::pair{Info::MediaType::audio, "audios"},
    std::pair{Info::MediaType::text, "texts"},
    std::pair{Info::MediaType::doc, "docs"},
  })json[key]=crow::json::wvalue::list(media_vector(mt).begin(),media_vector(mt).end());
  json["is_directory"]=is_directory;
  using namespace std::chrono;
  json["last_write_time"]=duration_cast<seconds>(last_write_time.time_since_epoch()).count();
  return json;
}

void Info::handle_filesystem_error(const std::error_code& ec, const std::string& operation) {
  last_error = ec;
  has_filesystem_error = true;
  
  // ログ出力
  std::cerr << "Filesystem error in " << operation 
            << " for path " << path.string() 
            << ": " << ec.message() << std::endl;
  
  // エラー種別に応じた処理
  if (ec == std::errc::permission_denied) {
    // 権限エラー: 部分的に処理を継続
  } else if (ec == std::errc::no_such_file_or_directory) {
    // ファイル不存在: 削除処理
  } else {
    // その他のエラー: 処理を停止
  }
}

bool Info::should_retry() const {
  if (!has_filesystem_error) return false;
  
  // 一時的なエラーかどうかを判定
  return last_error == std::errc::resource_unavailable_try_again ||
         last_error == std::errc::interrupted;
}

} // namespace VIEWER