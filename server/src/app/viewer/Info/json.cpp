#include <app/viewer/Info.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER{

unique_ptr<Info> Info::load(const crow::json::rvalue&json){
  std::unordered_map<uint64_t, Info*> id2info;
  return from_json(id2info, json);
}

unique_ptr<Info> Info::from_json(unordered_map<uint64_t, Info*>&id2info, const crow::json::rvalue&json){
  static constexpr std::array<const char*, 12> required_keys{{
    "id","par","path","tag","dirs",
    "imgs","videos","audios","texts","docs",
    "is_directory","last_write_time"
  }};
  { // 必須フィールドが全て揃っているか検証（不足していれば不適格として例外）
    std::string missing_keys;
    for(const auto* key : required_keys){
      if(!json.has(key))
        missing_keys += std::string(key) + ", ";
    }
    if(!missing_keys.empty())
      throw std::runtime_error("invalid dir_cache.json: missing key " + missing_keys);
  }

  unique_ptr<Info> info=make_unique<Info>();
  id2info[json["id"].u()]=info.get();
  info->par=id2info[json["par"].u()];
  info->path=Path(filesystem::path(json["path"].s()));
  for(const auto&tag:json["tag"].lo())
    info->tag.insert(tag.s());
  for(const auto&dir:json["dirs"].lo())
    info->dirs.push_back(from_json(id2info,dir));
  static auto media_push_back = [](auto&media, const auto&json){
    for(const auto&data:json)
      media.push_back(data.s());
  };
  for(auto&[mt, key]:std::array<std::pair<MediaType, const char*>, 5>{
    std::pair{MediaType::image, "imgs"},
    std::pair{MediaType::video, "videos"},
    std::pair{MediaType::audio, "audios"},
    std::pair{MediaType::text, "texts"},
    std::pair{MediaType::doc, "docs"},
  })media_push_back(info->media_vector(mt),json[key].lo());

  info->is_directory=json["is_directory"].b();
  using namespace std::chrono;
  info->last_write_time=filesystem::file_time_type::clock::time_point(seconds(json["last_write_time"].i()));
  manager& mgr = manager::get_instance();
  mgr.valid_info_ptrs.insert(info.get());
  if(info->has_only_img())
    mgr.leaf_dirs.insert(info.get());
  info->sort_dirs();
  info->sort_media_arrays();
  return info;
}

crow::json::wvalue Info::to_json()const{
  crow::json::wvalue json;
  crow::json::wvalue::list dirs_json;
  json["id"]=id();
  json["par"]=par->id();
  json["path"]=path.path.string();
  json["tag"]=crow::json::wvalue::list(tag.begin(),tag.end());
  for(const auto&dir:dirs)
    dirs_json.push_back(dir->to_json());
  json["dirs"]=crow::json::wvalue(dirs_json);
  for(auto&[mt, key]:std::array<std::pair<MediaType, const char*>, 5>{
    std::pair{MediaType::image, "imgs"},
    std::pair{MediaType::video, "videos"},
    std::pair{MediaType::audio, "audios"},
    std::pair{MediaType::text, "texts"},
    std::pair{MediaType::doc, "docs"},
  })json[key]=crow::json::wvalue::list(media_vector(mt).begin(),media_vector(mt).end());
  json["is_directory"]=is_directory;
  using namespace std::chrono;
  json["last_write_time"]=duration_cast<seconds>(last_write_time.time_since_epoch()).count();
  return json;
}
} // namespace VIEWER