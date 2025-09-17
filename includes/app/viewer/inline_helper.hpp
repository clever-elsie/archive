#pragma once
#include <app/viewer/global.hpp>
#include <crow/json.h>

namespace VIEWER{
using namespace std;

inline string rel_join(const string&dir){
	size_t start=0;
	while(dir[start]=='.'||dir[start]=='/')start++;
	return rel_base+string(dir.begin()+start,dir.end());
}

inline void pb_next(crow::json::wvalue::list&ret,const Info&info){
	if(info.imgs.size()){
		crow::json::wvalue next;
		next["img"]=filesystem::relative(filesystem::path(info.path)/info.imgs[0],rel_base);
		next["id"]=info.id;
		ret.push_back(next);
	}
}

// id(数値) → Info* 変換（0 は root_dir のエイリアス）
inline Info* get_info_from_id(uint64_t idv) noexcept{
	return idv==0 ? root_dir : reinterpret_cast<Info*>(idv);
}

} // namespace VIEWER