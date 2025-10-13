#include <algorithm>
#include <ranges>

#include <app/viewer/Info.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER{

Info::Info(const filesystem::path&dir,Info*par_)
:path(dir),tag(),
dirs(),imgs(),id(reinterpret_cast<uint64_t>(this)),par(par_?:this),has_only_img(0){
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.insert(this);
  last_write_time=filesystem::last_write_time(dir);
  if(!filesystem::is_directory(dir))return;
  reload_info();
  for(const auto&itr:filesystem::directory_iterator(dir))
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
  if(imgs.size()) last_write_time=filesystem::last_write_time(filesystem::path(path)/imgs[0]);
  std::ranges::sort(imgs);
  std::ranges::sort(dirs,[](const Info*a,const Info*b){
    const bool c1=filesystem::is_directory(a->path);
    const bool c2=filesystem::is_directory(b->path);
    if(c1==c2) return a->path<b->path;
    return c1;
  });
  has_only_img=!dirs.size();
  if(imgs.size()&&dirs.empty())
    mgr.leaf_dirs.insert(this);
}

Info::~Info(){
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.erase(this);
  if(imgs.size()&&dirs.empty())
    mgr.leaf_dirs.erase(this);
  for(auto&dir:dirs) delete dir;
}

bool Info::refresh(size_t depth){
  // この関数は常にmanager.imtexをlockした関数から呼び出す．
  // depth==0なら再帰を止める．
  // ただし，新しく観測したディレクトリはコンストラクタを呼び出すので全再帰
  manager& mgr = manager::get_instance();
  if(!filesystem::exists(path)){ // 消えてる
    delete this;
    return false;
  }
  if(imgs.size()&&dirs.empty()){ // 葉ノード
    filesystem::file_time_type last_write_time=filesystem::last_write_time(path/imgs[0]);
    if(last_write_time>this->last_write_time) // 更新有り
      reload_info(), reload_leaf(), this->last_write_time=last_write_time;
  }else if(filesystem::is_directory(path)){ // ディレクトリ
    filesystem::file_time_type last_write_time=filesystem::last_write_time(path);
    if(last_write_time>this->last_write_time) // 更新有り
      reload_dir(depth), reload_leaf(), this->last_write_time=last_write_time;
    else for(auto&dir:dirs) if(depth) dir->refresh(depth-1);
  }else{ // 動画・音声・テキストファイル は更新があってもやることなし
  }
  return true;
}

void Info::reload_info(){
  const filesystem::path info=this->path / ".info";
  tag.clear();
  if(filesystem::exists(info)){
    ifstream ifs(info);
    string buf;
    while(getline(ifs,buf)){
      if(buf.size()<1)continue;
      if(buf.back()=='\n')buf.pop_back();
      tag.emplace(buf);
    }
  }
}

void Info::reload_leaf(){
  imgs.clear();
  for(const auto&itr:filesystem::directory_iterator(this->path)){
    if(exts.end()!=std::ranges::find_if(exts,[&](const auto&ext){ return itr.path().extension().string()==ext; }))
      imgs.emplace_back(itr.path().filename().string());
  }
  std::ranges::sort(imgs);
}

void Info::reload_dir(size_t depth){
  vector<size_t> to_del;
  for(size_t i=0;i<dirs.size();++i){
    if(!filesystem::exists(dirs[i]->path)){
      to_del.push_back(i);
      continue;
    }
    if(depth) dirs[i]->refresh(depth-1);
  }
  for(auto rit=to_del.rbegin();rit!=to_del.rend();++rit){
    delete dirs[*rit];
    dirs.erase(dirs.begin()+*rit);
  } // 削除済みをキャッシュから削除
  vector<Info*> to_ins;
  for(auto it:filesystem::directory_iterator(path)){
    filesystem::path p(it.path());
    auto itr=std::lower_bound(dirs.begin(),dirs.end(),p,[](const Info*a, const filesystem::path&b){
      const bool c1=filesystem::is_directory(a->path);
      const bool c2=filesystem::is_directory(b);
      if(c1==c2) return a->path<b;
      return c1;
    });
    if(itr==dirs.end()||(*itr)->path!=p) // 新規
      to_ins.push_back(new Info(p, this));
  }
  dirs.insert(dirs.end(),to_ins.begin(),to_ins.end());
  std::ranges::sort(dirs,[](const Info*a,const Info*b){
    const bool c1=filesystem::is_directory(a->path);
    const bool c2=filesystem::is_directory(b->path);
    if(c1==c2) return a->path<b->path;
    return c1;
  });
}

} // namespace VIEWER