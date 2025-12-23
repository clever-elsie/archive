#include <crow/json.h>

#include <manager/auth/middleware.hpp>
#include <manager/auth/auth.hpp>
#include <manager/users/manager.hpp>
#include <app/viewer/tag.hpp>
#include <app/viewer/manager.hpp>
#include <app/viewer/inline_helper.hpp>

namespace VIEWER{
using namespace std;

crow::response info_renew(const crow::request&req){
	if (!USER_MANAGER::user_manager.is_admin(AUTH::get_username_from_token(MIDDLEWARE::extract_token(req))))
		return crow::response(403, crow::json::wvalue{crow::json::wvalue::object{{"error", "管理者権限が必要です"}}});
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	const auto data=crow::json::load(req.body);
	uint64_t idv=static_cast<uint64_t>(data["id"].i());
	string tar=data["data"].s();
	Info* node=mgr.get_info_from_id(idv);
	if(!mgr.is_valid(node) || !node->has_only_img()||!node->refresh(0)) return crow::response(404);
	if(data["AD"].s()=="add")
		return crow::response(node->add_tag(std::move(tar)));
	else return crow::response(node->remove_tag(tar));
}

} // namespace VIEWER