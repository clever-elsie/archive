#include <crow/json.h>

#include <manager/auth/middleware.hpp>
#include <manager/auth/auth.hpp>
#include <app/viewer/loader.hpp>
#include <app/viewer/global.hpp>

namespace VIEWER{
using namespace std;

void load_leaf_dir(const string&base){
	namespace C = std::chrono;
	namespace F = std::filesystem;
	leaf_dirs.clear();
	dirs_tree.clear();
	valid_info_ptrs.clear();
	delete root_dir;
	root_dir=new Info(base,nullptr);
	root_dir->par=root_dir;
	root_dir->id=reinterpret_cast<uint64_t>(root_dir);
	valid_info_ptrs.insert(root_dir);
	dirs_tree.insert(root_dir);
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
	
	lock_guard<mutex> lock(imtex);
	load_leaf_dir(base_dir);
	if(leaf_dirs.size()==0) return crow::response(400);
	return crow::response(200);
}
} // namespace VIEWER