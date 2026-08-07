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
	
	// reload用のmutexはtry_lockを使用し、既にリロード中なら何もせず正常終了（成功）とする
	std::unique_lock<std::mutex> reload_lock(mgr.reload_mutex, std::try_to_lock);
	if(!reload_lock.owns_lock()){
		CROW_LOG_INFO<<"load_leaf_dir skipped: reload already in progress";
		return;
	}

	bool is_initial = false;
	{
		std::lock_guard<std::mutex> lock(mgr.imtex);
		if(!mgr.root_dir){
			is_initial = true;
		}
	}

	if(is_initial){
		bool loaded = mgr.load_dir_cache(mgr.dir_cache_file);
		if(!loaded){
			// 新構築中（重いファイルスキャン処理）はimtexを保持しない
			auto new_root = make_unique<Info>(base, nullptr);
			if (mgr.is_stop_requested()) return;
			{
				std::lock_guard<std::mutex> lock(mgr.imtex);
				mgr.root_dir = std::move(new_root);
			}
			mgr.save_dir_cache(mgr.dir_cache_file);
			CROW_LOG_INFO<<"load_leaf_dir full scan done";
		}else{
			CROW_LOG_INFO<<"load_leaf_dir cache loaded";
		}
	}else{
		// 既存のキャッシュツリーを破棄して一からフルスキャンを実行
		// 1. 新rootの構築はアクセス権mutex (imtex) の外で行う
		auto new_root = make_unique<Info>(base, nullptr);
		if (mgr.is_stop_requested()) return;
		
		// 2. 新root完成後にアクセス権mutex (imtex) のロック下でポインタを交換
		unique_ptr<Info> old_root;
		{
			std::lock_guard<std::mutex> lock(mgr.imtex);
			old_root = std::move(mgr.root_dir);
			mgr.root_dir = std::move(new_root);
		}
		
		// 3. 旧rootの破棄はimtexのロック外で行う
		old_root.reset();
		
		mgr.save_dir_cache(mgr.dir_cache_file);
		CROW_LOG_INFO<<"load_leaf_dir reload full scan done";
	}
}

crow::response reload_leaf(const crow::request&req){
	manager& mgr = manager::get_instance();
	load_leaf_dir(mgr.base_dir);
	std::lock_guard<std::mutex> lock(mgr.imtex);
	if(mgr.trackable_trees[static_cast<size_t>(TreeType::all)].size()==0) return crow::response(400);
	return crow::response(200);
}
} // namespace VIEWER