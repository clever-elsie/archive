#include <crow/json.h>

#include <app/viewer/page.hpp>
#include <app/viewer/inline_helper.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER{
using namespace std;

crow::json::wvalue get_page_data(const crow::request&req){
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	const char* idx_c = req.url_params.get("idx");
	const char* page_size_c = req.url_params.get("page_size");
	if(!idx_c || !page_size_c)
		return crow::json::wvalue();

	int64_t idx;
	int64_t page_size;
	try {
		idx = std::stoll(idx_c);
		page_size = std::stoll(page_size_c);
	} catch(...) {
		return crow::json::wvalue();
	}
	
	crow::json::wvalue ret;
	const uint64_t n = mgr.leaf_dirs.size();
	const uint64_t page_cnt = (n + page_size - 1) / page_size;
	
	ret["total_pages"] = page_cnt;
	ret["page_size"] = page_size;
	ret["total_items"] = n;
	
	crow::json::wvalue::list items;
	if(0 <= idx && idx < (int64_t)page_cnt){
		uint64_t start = (uint64_t)idx * page_size;
		uint64_t end = min(n, start + page_size);
		for(uint64_t k = start; k < end; ++k){
			auto it = mgr.leaf_dirs.find_by_order(k);
			if(it != mgr.leaf_dirs.end()) {
				pb_next(items, (*it)->current_thumbnail_relative_path(), (*it)->id());
			}
		}
	}
	ret["items"] = crow::json::wvalue(items);
	return ret;
}

} // namespace VIEWER