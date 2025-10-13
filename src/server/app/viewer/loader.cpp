#include <crow/json.h>

#include <manager/auth/middleware.hpp>
#include <manager/auth/auth.hpp>
#include <app/viewer/loader.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER{
using namespace std;

void load_leaf_dir(const string&base){
	namespace C = std::chrono;
	namespace F = std::filesystem;
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	delete mgr.root_dir;
	mgr.root_dir=new Info(base,nullptr);
}

crow::response reload_leaf(const crow::request&req){
	// 管理者権限チェック
	string token = MIDDLEWARE::extract_token(req);
	if (token.empty() || !AUTH::validate_token_wrapper(token)) {
		crow::json::wvalue error_response;
		error_response["error"] = "認証が必要です";
		return crow::response(401, error_response);
	}
	
	string username = AUTH::get_username_from_token(token);
	if (!USER_MANAGER::user_manager.is_admin(username)) {
		crow::json::wvalue error_response;
		error_response["error"] = "管理者権限が必要です";
		return crow::response(403, error_response);
	}
	
	manager& mgr = manager::get_instance();
	load_leaf_dir(mgr.base_dir);
	if(mgr.leaf_dirs.size()==0) return crow::response(400);
	return crow::response(200);
}
} // namespace VIEWER