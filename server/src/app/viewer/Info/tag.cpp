#include <utility>
#include <fstream>
#include <filesystem>
#include <inline_helper.hpp>
#include <app/viewer/Info.hpp>

namespace VIEWER{

int Info::add_tag(std::string&& tag){
  if(this->tag.contains(tag)) return 200;
  std::filesystem::path info=this->path/".info";
  std::ofstream ofs(info,ios_base::app);
  ofs<<tag<<'\n';
  this->tag.emplace(std::move(tag));
  return 201;
}

int Info::remove_tag(const std::string& tag){
  if(!this->tag.contains(tag)) return 200;
  this->tag.erase(tag);
  std::filesystem::path info=this->path/".info";
  std::ofstream ofs(info,ios_base::trunc);
  for(const auto&x:this->tag)
    ofs<<x<<'\n';
  return 200;
}

const std::vector<std::string> Info::normalized_tags()const{
  std::vector<std::string> ret;
  for(const auto&elem:this->tag)
    ret.push_back(html_escape(elem));
  return ret;
}

} // namespace VIEWER