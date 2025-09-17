#include <crow/json.h>

#include <manager/auth/middleware.hpp>
#include <manager/auth/auth.hpp>
#include <manager/users/manager.hpp>
#include <app/viewer/tag.hpp>
#include <app/viewer/Info.hpp>
#include <app/viewer/global.hpp>
#include <app/viewer/inline_helper.hpp>

namespace VIEWER{
using namespace std;

crow::response info_renew(const crow::request&req){
	string token = MIDDLEWARE::extract_token(req);
	string username = AUTH::get_username_from_token(token);
	if (!USER_MANAGER::user_manager.is_admin(username)) {
		crow::json::wvalue error_response;
		error_response["error"] = "管理者権限が必要です";
		return crow::response(403, error_response);
	}
	lock_guard<mutex> lock(imtex);
	const auto data=crow::json::load(req.body);
	uint64_t idv=static_cast<uint64_t>(data["id"].i());
	string tar=data["data"].s();
	Info* node=get_info_from_id(idv);
	if(!node || !valid_info_ptrs.contains(node) || !node->has_only_img) return crow::response(404);
	string info=node->path+"/.info";
	if(data["AD"].s()=="add"){
		if(node->tag.contains(tar)) return crow::response(200);
		node->tag.emplace(move(tar));
		ofstream ofs(info,ios_base::app);
		ofs<<tar<<'\n';
	}else{ // delete
		node->tag.erase(tar);
		ofstream ofs(info,ios_base::trunc);
		for(const auto&x:node->tag)
			ofs<<x<<'\n';
	}
	return crow::response(200);
}

} // namespace VIEWER