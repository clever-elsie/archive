#include <app/viewer/Info.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER{

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
  auto exists_result = SafeFS::exists(path.path);
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
    auto time_result = SafeFS::last_write_time(path.path/media_vector<MediaType::image>()[0]);
    if(!time_result.success()){
      handle_filesystem_error(time_result.ec, "last_write_time (leaf)");
      refresh_from_parent();
      return false;
    }
    auto current_time = time_result.value;
    std::error_code ec;
    auto info_time = std::filesystem::last_write_time(path.path / ".info", ec);
    if (!ec) {
      current_time = std::max(current_time, info_time);
    }
    if(current_time > this->last_write_time){ // 更新有り
      mgr.leaf_dirs.erase(this);
      reload_info(), reload_leaf();
      this->last_write_time = current_time;
      has_update = true;
      mgr.leaf_dirs.insert(this);
    }
  }else if(is_directory){ // ディレクトリ
    auto time_result = SafeFS::last_write_time(path.path);
    if(!time_result.success()){
      handle_filesystem_error(time_result.ec, "last_write_time (directory)");
      // 自分自身の情報を取得できないとき，他のタスクがディレクトリをロックしている
      // したがって，アクセスできないので，親からrefreshして削除してもらう
      refresh_from_parent();
      return false;
    }
    auto current_time = time_result.value;
    std::error_code ec;
    auto info_time = std::filesystem::last_write_time(path.path / ".info", ec);
    if (!ec) {
      current_time = std::max(current_time, info_time);
    }
    if(current_time > this->last_write_time){ // 更新有り
      reload_dir(depth), reload_leaf(), reload_info();
      this->last_write_time = current_time;
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
  const filesystem::path info=this->path.path / ".info";
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
  auto iter_result = SafeFS::directory_iterator(this->path.path);
  if(!iter_result.success()){
    handle_filesystem_error(iter_result.ec, "reload_leaf directory_iterator");
    return;
  }
  Info::MediaArray new_media;
  for(const auto&itr:iter_result.value)
    classify_and_push(itr.path(), new_media);
  media = std::move(new_media);
  sort_media_arrays();
}

void Info::reload_dir(size_t depth){
  vector<size_t> to_del;
  for(size_t i=0;i<dirs.size();){
    auto exists_result = SafeFS::exists(dirs[i]->path.path);
    if (!exists_result.success()) {
      handle_filesystem_error(exists_result.ec, "reload_dir exists");
      ++i;
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
      auto exists_result = SafeFS::exists(filesystem::path(this->path.path)/files[i]);
      if(!exists_result.success()){
        handle_filesystem_error(exists_result.ec, op_name);
        ++i;
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
  for(std::size_t i=0;i<media_type_count();++i){
    auto mt = static_cast<MediaType>(i);
    erase_if_missing(media[Info::mt_index(mt)], Info::mt_string(mt).data());
  }
  // ここでのソートは一意に決定できなければならないので，pathを用いる．sortkeyは一意ではない．
  std::ranges::sort(dirs,[](const std::unique_ptr<Info>&a, const std::unique_ptr<Info>&b){
    return a->path<b->path;
  });
  sort_media_arrays();
  vector<unique_ptr<Info>> to_ins;
  Info::MediaArray to_ins_media{};
  auto iter_result = SafeFS::directory_iterator(path.path);
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
      auto itr_pos=std::lower_bound(dirs.begin(),dirs.end(),p,[](const std::unique_ptr<Info>&a, const filesystem::path&b){
        return a->path<b;
      });
      if(itr_pos==dirs.end()||(*itr_pos)->path!=p){// 新規
        auto n = make_unique<Info>(p, this);
        if(n && !n->empty())
          to_ins.push_back(std::move(n));
      }
    }else{
      // 画像 / 動画 / 音声 / テキスト / ドキュメント へ振り分け（差分用）
      classify_and_push(itr.path(), this->media, to_ins_media);
    }
  }
  dirs.insert(dirs.end(),std::make_move_iterator(to_ins.begin()),std::make_move_iterator(to_ins.end()));
  for(std::size_t i=0;i<media_type_count();++i){
    auto& dest = media[i];
    auto& src  = to_ins_media[i];
    dest.insert(dest.end(), std::make_move_iterator(src.begin()), std::make_move_iterator(src.end()));
  }
  sort(); // ここのソートはsortkey
}

} // namespace VIEWER