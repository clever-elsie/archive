#include <algorithm>
#include <ranges>
#include <app/viewer/Info.hpp>

namespace VIEWER{

void Info::sort(){
  sort_dirs();
  sort_media_arrays();
}

void Info::sort(std::vector<Info*>&vec, SortingOrder order, bool descendant){
  if(order==SortingOrder::last_write_time){
    auto cmp=[](const Info*a,const Info*b){
      if(a->last_write_time==b->last_write_time)
        return a->path<b->path;
      return a->last_write_time<b->last_write_time;
    };
    std::ranges::sort(vec, cmp);
  }else
    std::ranges::sort(vec,[](const Info*a,const Info*b){
      return a->path<b->path;
    });
  if(descendant)
    std::ranges::reverse(vec);
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

} // namespace VIEWER