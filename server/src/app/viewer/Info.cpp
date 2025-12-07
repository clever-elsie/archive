#include <algorithm>
#include <app/viewer/Info.hpp>
#include <app/viewer/manager.hpp>
#include <app/viewer/safe_filesystem.hpp>
#include <ranges>
#include <system_error>
#include <iostream>
#include <string_view>

namespace VIEWER{

namespace {

  using MT = Info::MediaType;
  constexpr std::size_t MediaTypeCount = Info::media_type_count();

  struct MediaExtConfig {
    std::array<std::string_view, 5> exts{};
    std::size_t count{};
  };

  // MediaType ごとの拡張子定義
  constexpr std::array<MediaExtConfig, MediaTypeCount> MEDIA_EXTS{{
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

  inline const MediaExtConfig& exts_for(MT t){
    return MEDIA_EXTS[Info::mt_index(t)];
  }

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
  template <class MediaArray>
  void classify_and_push(const std::string& file_ext,
                         const std::string& filename,
                         MediaArray& media){
    for(std::size_t i=0;i<MediaTypeCount;++i){
      auto mt = static_cast<MT>(i);
      const auto& cfg = MEDIA_EXTS[i];
      for(std::size_t j=0;j<cfg.count;++j){
        if(file_ext == cfg.exts[j]){
          media[i].emplace_back(filename);
          return;
        }
      }
    }
    // どのカテゴリにもマッチしないレギュラーファイルは無視
  }

  template <class MediaArray>
  void sort_media_arrays(MediaArray& media){
    for(auto& v : media) std::ranges::sort(v);
  }
}

Info::Info(const filesystem::path&dir,Info*par_)
:path(dir),tag(),
dirs(),media(),par(par_?:this),
is_directory(false),has_filesystem_error(false){
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.insert(this);
  
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
  
  // 安全なdirectory_iterator作成
  auto iter_result = SafeFS::directory_iterator(dir);
  if (!iter_result.success()) {
    handle_filesystem_error(iter_result.ec, "directory_iterator");
    return;
  }
  
  for(const auto&itr:iter_result.value){
    if(itr.is_directory()){
      Info *n=new Info(itr.path(),this);
      if(n && (n->has_any_media() || !n->dirs.empty()))
        dirs.push_back(n);
      else delete n;
    }else{
      const auto file_ext = itr.path().extension().string();
      const auto filename = itr.path().filename().string();
      // 画像 / 動画 / 音声 / テキスト / ドキュメント へ振り分け
      classify_and_push(file_ext, filename, media);
    }
  }
  if(!imgs().empty()) {
    auto img_time_result = SafeFS::last_write_time(filesystem::path(path)/imgs()[0]);
    if (img_time_result.success())
      last_write_time = img_time_result.value;
    else{
      handle_filesystem_error(img_time_result.ec, "last_write_time (leaf)");
      imgs().clear();
      dirs.clear();
      // これはディレクトリなので両方空なら呼出し元にdeleteされる
    }
  }
  std::ranges::sort(imgs());
  sort_dirs();
  if(has_only_img())
    mgr.leaf_dirs.insert(this);
}

Info::~Info(){
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.erase(this);
  if(has_only_img())
    mgr.leaf_dirs.erase(this);
  for(auto&dir:dirs) delete dir;
}

void Info::sort_dirs(){
  std::ranges::sort(dirs,[](const Info*a,const Info*b){
    if(a->is_directory==b->is_directory)
      return a->path<b->path;
    return a->is_directory;
  });
}

bool Info::refresh_from_parent(){
  if(par!=this) return par->refresh_from_parent();
  else{
    delete this;
    manager::get_instance().mark_cache_dirty();
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
  vector<string> new_imgs;
  const auto& cfg = exts_for(MT::image);
  for(const auto&itr:iter_result.value){
    const auto ext = itr.path().extension().string();
    for(std::size_t i=0;i<cfg.count;++i){
      if(ext == cfg.exts[i]){
        new_imgs.emplace_back(itr.path().filename().string());
        break;
      }
    }
  }
  imgs() = std::move(new_imgs);
  std::ranges::sort(imgs());
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
      delete dirs.back();
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
  sort_media_arrays(media);
  vector<Info*> to_ins;
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
    const auto file_ext = p.extension().string();
    const auto filename = p.filename().string();
    // ディレクトリ
    if(is_dir_result.value){
      using ppt = pair<filesystem::path, bool>;
      ppt pp(p, is_dir_result.value);
      auto itr_pos=std::lower_bound(dirs.begin(),dirs.end(),pp,[](const Info*a, const ppt&b){
        if(a->is_directory==b.second) return a->path<b.first;
        return a->is_directory;
      });
      if(itr_pos==dirs.end()||(*itr_pos)->path!=p) // 新規
        to_ins.push_back(new Info(p, this));
    }else{
      // 画像 / 動画 / 音声 / テキスト / ドキュメント へ振り分け（差分用）
      classify_and_push(file_ext, filename, to_ins_media);
    }
  }
  dirs.insert(dirs.end(),to_ins.begin(),to_ins.end());
  sort_dirs();
  for(std::size_t i=0;i<MediaTypeCount;++i){
    auto mt = static_cast<MT>(i);
    auto& dest = media[Info::mt_index(mt)];
    auto& src  = to_ins_media[i];
    dest.insert(dest.end(), src.begin(), src.end());
  }
  sort_media_arrays(media);
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