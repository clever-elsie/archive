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

} // namespace VIEWER