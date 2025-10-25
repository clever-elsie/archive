#include <crow/json.h>

#include <app/viewer/page.hpp>
#include <app/viewer/inline_helper.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER{
using namespace std;

crow::json::wvalue get_page_data(const crow::request&req){
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	auto data = crow::json::load(req.body);
	if(!data.has("idx")||!data.has("page_size"))
		return crow::json::wvalue();

	int64_t idx = data["idx"].i();
	int64_t page_size = data["page_size"].i();
	
	crow::json::wvalue ret;
	uint64_t n = mgr.leaf_dirs.size();
	uint64_t page_cnt = (n + page_size - 1) / page_size;
	
	ret["total_pages"] = page_cnt;
	ret["page_size"] = page_size;
	ret["total_items"] = n;
	
	// idx=-1の場合はページ情報のみを返す（itemsは空の配列）
	if(idx == -1){
		ret["items"] = crow::json::wvalue::list();
		return ret;
	}
	
	crow::json::wvalue::list items;
	if(0 <= idx && idx < (int64_t)page_cnt){
		uint64_t start = (uint64_t)idx * page_size;
		uint64_t end = min(n, start + page_size);
		for(uint64_t k = start; k < end; ++k){
			auto it = mgr.leaf_dirs.find_by_order(k);
			if(it != mgr.leaf_dirs.end())
				if((*it)->refresh(0))
					pb_next(items, **it);
				else --k;
		}
	}
	ret["items"] = crow::json::wvalue(items);
	return ret;
}

} // namespace VIEWER