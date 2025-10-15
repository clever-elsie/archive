#pragma once
#include <crow/json.h>
#include <app/viewer/manager.hpp>

namespace VIEWER{
using namespace std;

inline void pb_next(crow::json::wvalue::list&ret,const Info&info){
	const manager&mgr = manager::get_instance();
	if(info.imgs.size()){
		crow::json::wvalue next;
		next["img"]=filesystem::relative(filesystem::path(info.path)/info.imgs[0],mgr.base_dir);
		next["id"]=info.id();
		ret.push_back(next);
	}
}

} // namespace VIEWER