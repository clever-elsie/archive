#include <algorithm>
#include <app/viewer/Info.hpp>
#include <app/viewer/manager.hpp>
#include <app/viewer/safe_filesystem.hpp>
#include <ranges>
#include <system_error>
#include <iostream>

namespace VIEWER{

Info::Info(const filesystem::path&dir,Info*par_)
:path(dir),tag(),
dirs(),imgs(),par(par_?:this),
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
  
  for(const auto&itr:iter_result.value)
    if(itr.is_directory()){
      Info *n=new Info(itr.path(),this);
      if(n&&(n->imgs.size()||n->dirs.size()))
        dirs.push_back(n);
      else delete n;
    }else{
      const auto&file_ext=itr.path().extension().string();
      if(exts.end()!=std::ranges::find_if(exts,[&](const auto&ext){ return file_ext==ext; }))
        imgs.emplace_back(itr.path().filename().string());
      else if(not_img.end()!=std::ranges::find_if(not_img,[&](const auto&ext){ return file_ext==ext; }))
        dirs.push_back(new Info(itr.path(),this));
    }
  if(imgs.size()) {
    auto img_time_result = SafeFS::last_write_time(filesystem::path(path)/imgs[0]);
    if (img_time_result.success())
      last_write_time = img_time_result.value;
    else{
      handle_filesystem_error(img_time_result.ec, "last_write_time (leaf)");
      imgs.clear();
      dirs.clear();
      // これはディレクトリなので両方空なら呼出し元にdeleteされる
    }
  }
  std::ranges::sort(imgs);
  std::ranges::sort(dirs,[](const Info*a,const Info*b){
    if(a->is_directory==b->is_directory)
      return a->path<b->path;
    return a->is_directory;
  });
  if(has_only_img())
    mgr.leaf_dirs.insert(this);
}

Info::~Info(){
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.erase(this);
  if(imgs.size()&&dirs.empty())
    mgr.leaf_dirs.erase(this);
  for(auto&dir:dirs) delete dir;
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
  if(imgs.size()&&dirs.empty()){ // 葉ノード
    auto time_result = SafeFS::last_write_time(path/imgs[0]);
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
    } else if(depth)
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
  for(const auto&itr:iter_result.value)
    if(std::ranges::contains(exts,itr.path().extension().string()))
      new_imgs.emplace_back(itr.path().filename().string());
  std::ranges::sort(new_imgs);
  imgs=std::move(new_imgs);
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
  for(size_t i=0;i<imgs.size();){
    auto exists_result = SafeFS::exists(filesystem::path(this->path)/imgs[i]);
    if(!exists_result.success()){
      handle_filesystem_error(exists_result.ec, "reload_dir exists (leaf)");
      continue;
    }
    if(!exists_result.value){
      std::swap(imgs[i],imgs.back());
      imgs.pop_back();
      continue;
    }
    ++i;
  }// 削除済み画像を削除
  vector<Info*> to_ins;
  vector<string> to_ins_img;
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
    // 画像
    if(ranges::find_if(exts,[fext=p.extension().string()](const auto&ext){ return fext==ext; })){
      auto itr_pos=std::lower_bound(imgs.begin(),imgs.end(),p.filename().string());
      if(itr_pos==imgs.end()||(*itr_pos)!=p.filename().string()) // 新規
        to_ins_img.push_back(p.filename().string());
    }else{ // それ以外
      using ppt = pair<filesystem::path, bool>;
      ppt pp(p, is_dir_result.value);
      auto itr_pos=std::lower_bound(dirs.begin(),dirs.end(),pp,[](const Info*a, const ppt&b){
        if(a->is_directory==b.second) return a->path<b.first;
        return a->is_directory;
      });
      if(itr_pos==dirs.end()||(*itr_pos)->path!=p) // 新規
        to_ins.push_back(new Info(p, this));
    }
  }
  dirs.insert(dirs.end(),to_ins.begin(),to_ins.end());
  std::ranges::sort(dirs,[](const Info*a,const Info*b){
    if(a->is_directory==b->is_directory) return a->path<b->path;
    return a->is_directory;
  });
  imgs.insert(imgs.end(),to_ins_img.begin(),to_ins_img.end());
  std::ranges::sort(imgs);
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
  json["imgs"]=crow::json::wvalue::list(imgs.begin(),imgs.end());
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