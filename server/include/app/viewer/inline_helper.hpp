#pragma once
#include <crow/json.h>
#include <app/viewer/manager.hpp>

namespace VIEWER{
using namespace std;

inline void pb_next(crow::json::wvalue::list&ret,const std::string&path, size_t id){
	crow::json::wvalue next;
	next["img"]=path;
	next["id"]=id;
	ret.push_back(next);
}

inline void pb_next(crow::json::wvalue::list&ret, const Info* node) {
	crow::json::wvalue next;
	next["id"] = node->id();
	next["dirname"] = node->dirname();
	
	auto type = node->directory_type();
	next["dir_type"] = Info::directory_type_to_string(type);
	
	if (node->is_trackable()) {
		next["click_action"] = "gallery";
		next["img"] = node->current_thumbnail_relative_path();
	} else {
		next["click_action"] = "navigate";
		next["img"] = "";
	}
	ret.push_back(std::move(next));
}

inline void pb_next(crow::json::wvalue::list&ret, const VideoFile* vf) {
	crow::json::wvalue next;
	next["id"] = vf->id();
	next["dirname"] = vf->name;
	next["click_action"] = "play_media";
	next["media_type"] = "video";
	next["media_path"] = vf->path;
	next["dir_type"] = "file";
	next["img"] = "";
	ret.push_back(std::move(next));
}

} // namespace VIEWER