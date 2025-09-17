#include <algorithm>
#include <ranges>

#include <app/viewer/Info.hpp>
#include <app/viewer/global.hpp>

namespace VIEWER{

Info::Info(const string&dir,Info*par_)
:path(dir),tag(),
dirs(),imgs(),id(UINT64_MAX),par(par_),has_only_img(0){
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
      if(n!=nullptr&&(n->imgs.size()||n->dirs.size())){
        dirs.push_back(n);
        n->id=reinterpret_cast<uint64_t>(n);
        valid_info_ptrs.insert(n);
        if(n->dirs.size()) dirs_tree.insert(n);
        else leaf_dirs.insert(n);
      }else delete n;
    }else{
      for(const auto&ext:exts)
        if(const auto s=itr.path().filename().string();s.ends_with(ext)){
          imgs.emplace_back(s);
          break;
        }
      for(const auto&ext:not_img)
        if(const auto s=itr.path().string();s.ends_with(ext)){
          Info *n=new Info(s,this);
          dirs.push_back(n);
          break;
        }
    }
  if(imgs.size()) last_write_time=filesystem::last_write_time(filesystem::path(path)/imgs[0]);
  sort(imgs.begin(),imgs.end());
  sort(dirs.begin(),dirs.end(),[](const Info*a,const Info*b){
    bool c1=filesystem::is_directory(a->path);
    bool c2=filesystem::is_directory(b->path);
    if(c1==c2) return a->path<b->path;
    return c1;
  });
  has_only_img=!dirs.size();
}

} // namespace VIEWER