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
	if(mgr.root_dir==nullptr){
		if(!mgr.load_dir_cache(mgr.dir_cache_file)){
			mgr.root_dir=new Info(base,nullptr);
			lock.~lock_guard();
			mgr.save_dir_cache(mgr.dir_cache_file);
		}else{
			// キャッシュから読み込んだ場合、バックグラウンドでフルスキャンを実行
			mgr.trigger_full_scan_if_needed();
		}
	}else mgr.root_dir->refresh(998244353ul/*this number is no means if you want to more depth, you can change it*/);
	
	// キャッシュ監視を開始
	mgr.start_cache_monitor();
}

crow::response reload_leaf(const crow::request&req){
	// 管理者権限チェック
	string token = MIDDLEWARE::extract_token(req);
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