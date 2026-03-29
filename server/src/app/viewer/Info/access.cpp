#include <vector>
#include <filesystem>
#include <ranges>
#include <algorithm>
#include <system_error>
#include <app/viewer/manager.hpp>
#include <app/viewer/Info.hpp>

namespace VIEWER{

std::filesystem::path Info::locate_media(MediaType type, const std::string& filename)const{
  manager&mgr=manager::get_instance();
  std::error_code ec;
  auto canon_base = std::filesystem::weakly_canonical(std::filesystem::path(mgr.base_dir), ec);
  auto path = this->path.path/filename;
  auto canon_path = std::filesystem::weakly_canonical(path, ec);
  if(ec || canon_path.empty() || canon_base.empty())
    throw 404;
  if(!canon_path.generic_string().starts_with(canon_base.generic_string()))
    throw 403;
  auto ret=std::filesystem::relative(canon_path, canon_base, ec);
  if(ec) throw 404;
  return ret;
}

std::string Info::current_thumbnail_relative_path()const{
  const auto& imgs=this->media_vector<MediaType::image>();
  if(imgs.empty()) return "";
  manager&mgr=manager::get_instance();
  return std::filesystem::relative(filesystem::path(this->path.path)/imgs[0],mgr.base_dir).string();
}

std::vector<std::string> Info::all_thumbnail_relative_paths()const{
  std::vector<std::string> ret;
  for(const auto&dir:this->dirs)
    ret.push_back(dir->current_thumbnail_relative_path());
  return ret;
}

std::string Info::parent_thumbnail_relative_path()const{
  return this->par->current_thumbnail_relative_path();
}

std::vector<std::string> Info::parent_all_thumbnail_relative_paths()const{
  return this->par->all_thumbnail_relative_paths();
}

template<Info::MediaType type>
std::vector<std::string> Info::media_relative_paths(SortingOrder order, bool descendant)const{
  std::vector<std::string> ret = media_vector<type>();
  manager&mgr=manager::get_instance();
  for(auto& elem:ret)
    elem=(this->path.path/elem).string();
  if(order==SortingOrder::last_write_time){
    std::ranges::sort(ret,[](const auto&a,const auto&b){
        auto at=filesystem::last_write_time(a);
        auto bt=filesystem::last_write_time(b);
        if(at==bt) return a<b;
        return at<bt;
    });
  }
  if(descendant) std::ranges::reverse(ret);
  for(auto&elem:ret)
    elem=std::filesystem::path(elem).lexically_relative(mgr.base_dir).string();
  return ret;
}

std::vector<std::string> Info::media_relative_paths(Info::MediaType type, SortingOrder order, bool descendant)const{
  if(type==MediaType::image) return media_relative_paths<MediaType::image>(order,descendant);
  if(type==MediaType::video) return media_relative_paths<MediaType::video>(order,descendant);
  if(type==MediaType::audio) return media_relative_paths<MediaType::audio>(order,descendant);
  if(type==MediaType::text) return media_relative_paths<MediaType::text>(order,descendant);
  if(type==MediaType::doc) return media_relative_paths<MediaType::doc>(order,descendant);
  return std::vector<std::string>();
}

} // namespace VIEWER