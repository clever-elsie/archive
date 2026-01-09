#include <algorithm>
#include <ranges>
#include <compare>
#include <app/viewer/Info.hpp>

namespace VIEWER{

void Info::sort(){
  sort_dirs();
  sort_media_arrays();
}

template<class T>
concept is_info_pointer=std::same_as<std::decay_t<T>, Info*> || std::same_as<std::decay_t<T>, std::unique_ptr<Info>>;

template<is_info_pointer T>
void sort_impl(std::vector<T>&vec, Info::SortingOrder order, bool descendant){
  // 並びの一意性のために，もしsortkeyが同じときはpathを比較する．
  if(order==Info::SortingOrder::last_write_time){
    auto cmp=[](const T& a,const T& b){
      if(a->last_write_time_value()==b->last_write_time_value()){
        auto c = a->sortkey_value()<=>b->sortkey_value();
        if(c!=0) return c<0;
        return a->full_path()<b->full_path();
      }
      return a->last_write_time_value()<b->last_write_time_value();
    };
    std::ranges::sort(vec, cmp);
  }else
    std::ranges::sort(vec,[](const T& a,const T& b){
      auto c = a->sortkey_value()<=>b->sortkey_value();
      if(c!=0) return c<0;
      return a->full_path()<b->full_path();
    });
  if(descendant)
    std::ranges::reverse(vec);
}

void Info::sort(std::vector<Info*>&vec, SortingOrder order, bool descendant){
  sort_impl(vec, order, descendant);
}

void Info::sort_dirs(){ 
  sort_impl(dirs, SortingOrder::name, false);
}

void Info::sort_media_arrays(){
  for(auto& v : this->media){
    std::ranges::sort(v);
    v.erase(std::unique(v.begin(),v.end()),v.end());
  }
}

} // namespace VIEWER