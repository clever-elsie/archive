#include <utility>
#include <vector>
#include <ranges>
#include <algorithm>
#include <app/viewer/Info.hpp>

namespace VIEWER{

std::pair<std::vector<Info*>, std::vector<Info*>>
Info::imgdirs_or_elsedirs(SortingOrder order, bool descendant)const{
  std::vector<Info*> dirvec,imgvec;
  for(const auto&d:this->dirs)
    (d->has_only_img()?imgvec:dirvec).push_back(d.get());
  if(order==SortingOrder::last_write_time){
    auto cmp=[](const Info*a,const Info*b){
      if(a->last_write_time==b->last_write_time)
        return a->path<b->path;
      return a->last_write_time<b->last_write_time;
    };
    std::ranges::sort(dirvec,cmp);
    std::ranges::sort(imgvec,cmp);
  }
  if(descendant){
    std::ranges::reverse(dirvec);
    std::ranges::reverse(imgvec);
  }
  return std::make_pair(std::move(imgvec),std::move(dirvec));
}
} // namespace VIEWER