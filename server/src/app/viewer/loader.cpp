#include <crow/logging.h>
#include <crow/json.h>

#include <app/viewer/loader.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER{
using namespace std;

void load_leaf_dir(const string&base){
	namespace C = std::chrono;
	namespace F = std::filesystem;
	manager& mgr = manager::get_instance();
	// まずロックを取るが、キャッシュI/Oはロックの外で行う
	std::unique_lock<std::mutex> lock(mgr.imtex);
	if(!mgr.root_dir){
		// ロックを外してキャッシュ読み込み（内側でimtexを取るため）
		lock.unlock();
		bool loaded = mgr.load_dir_cache(mgr.dir_cache_file);
		if(!loaded){
			// root_dir の作成はロック下で二重チェック
			lock.lock();
			if(!mgr.root_dir)
				mgr.root_dir=make_unique<Info>(base,nullptr);
			lock.unlock();
			// キャッシュ保存はロックの外で（内部でimtexを取得）
			mgr.save_dir_cache(mgr.dir_cache_file);
			CROW_LOG_INFO<<"load_leaf_dir full scan done";
		}else{
			CROW_LOG_INFO<<"load_leaf_dir cache loaded";
			// キャッシュから読み込んだ場合、バックグラウンドでフルスキャンを実行
			mgr.trigger_full_scan_if_needed();
		}
	}else{
		// refresh は imtex を保持した呼び出し元からのみ呼ぶ契約
		mgr.root_dir->refresh(998244353ul/*this number is no means if you want to more depth, you can change it*/);
		CROW_LOG_INFO<<"load_leaf_dir refresh differential done";
	}
	// キャッシュ監視を開始
	mgr.start_cache_monitor();
}

crow::response reload_leaf(const crow::request&req){
	manager& mgr = manager::get_instance();
	load_leaf_dir(mgr.base_dir);
	if(mgr.leaf_dirs.size()==0) return crow::response(400);
	return crow::response(200);
}
} // namespace VIEWER