#include <crow/json.h>

#include <app/viewer/page.hpp>
#include <app/viewer/inline_helper.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER{
using namespace std;

namespace {
TreeType parse_filter(const char* filter_c) {
	if (!filter_c) return TreeType::all;
	std::string_view f(filter_c);
	if (f == "images") return TreeType::images;
	if (f == "movies") return TreeType::movies;
	if (f == "texts") return TreeType::texts;
	if (f == "pdfs") return TreeType::pdfs;
	if (f == "musics") return TreeType::musics;
	return TreeType::all;
}
}

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
	
	const char* filter_c = req.url_params.get("filter");
	TreeType filter_type = parse_filter(filter_c);
	const auto& target_tree = mgr.trackable_trees[static_cast<size_t>(filter_type)];
	
	crow::json::wvalue ret;
	const uint64_t n = target_tree.size();
	const uint64_t page_cnt = (n + page_size - 1) / page_size;
	
	ret["total_pages"] = page_cnt;
	ret["page_size"] = page_size;
	ret["total_items"] = n;
	
	crow::json::wvalue::list items;
	if(0 <= idx && idx < (int64_t)page_cnt){
		uint64_t start = (uint64_t)idx * page_size;
		uint64_t end = min(n, start + page_size);
		for(uint64_t k = start; k < end; ++k){
			auto it = target_tree.find_by_order(k);
			if(it != target_tree.end()) {
				pb_next(items, *it);
			}
		}
	}
	ret["items"] = crow::json::wvalue(items);
	return ret;
}

} // namespace VIEWER