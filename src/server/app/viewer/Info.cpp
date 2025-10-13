#include <algorithm>
#include <ranges>

#include <app/viewer/Info.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER{

Info::Info(const string&dir,Info*par_)
:path(dir),tag(),
dirs(),imgs(),id(reinterpret_cast<uint64_t>(this)),par(par_?:this),has_only_img(0){
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.insert(this);
  last_write_time=filesystem::last_write_time(dir);
  if(!filesystem::is_directory(dir))return;
  const string info=dir+"/.info";
  if(filesystem::exists(info)){
    ifstream ifs(info);
    string buf;
    while(getline(ifs,buf)){
      if(buf.size()<1)continue;
      if(buf.back()=='\n')buf.pop_back();
      tag.emplace(buf);
    }
  }
  constexpr static array<string,5> exts{".webp",".jpg",".jpeg",".png",".gif"};
  constexpr static array<string,6> not_img{".mp4",".mp3",".flac",".aac",".wav",".txt"};
  for(const auto&itr:filesystem::directory_iterator(dir))
    if(itr.is_directory()){
      Info *n=new Info(itr.path().string(),this);
      if(n&&(n->imgs.size()||n->dirs.size()))
        dirs.push_back(n);
      else delete n;
    }else{
      const auto&file_ext=itr.path().extension().string();
      if(exts.end()!=std::ranges::find_if(exts,[&](const auto&ext){ return file_ext==ext; }))
        imgs.emplace_back(itr.path().filename().string());
      else if(not_img.end()!=std::ranges::find_if(not_img,[&](const auto&ext){ return file_ext==ext; }))
        dirs.push_back(new Info(itr.path().string(),this));
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

} // namespace VIEWER