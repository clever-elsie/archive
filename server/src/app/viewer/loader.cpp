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
		}
	}else{
		// 既存のキャッシュツリーを破棄して一からフルスキャンを実行
		mgr.root_dir.reset();
		mgr.root_dir=make_unique<Info>(base,nullptr);
		lock.unlock();
		mgr.save_dir_cache(mgr.dir_cache_file);
		CROW_LOG_INFO<<"load_leaf_dir reload full scan done";
	}
}

crow::response reload_leaf(const crow::request&req){
	manager& mgr = manager::get_instance();
	load_leaf_dir(mgr.base_dir);
	if(mgr.trackable_trees[static_cast<size_t>(TreeType::all)].size()==0) return crow::response(400);
	return crow::response(200);
}
} // namespace VIEWER