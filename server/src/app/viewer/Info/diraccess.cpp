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
    (d->media_vector<MediaType::image>().empty()?dirvec:imgvec).push_back(d.get());
  sort(dirvec,order,descendant); // Info/sort.cpp
  sort(imgvec,order,descendant);
  return std::make_pair(std::move(imgvec),std::move(dirvec));
}
} // namespace VIEWER