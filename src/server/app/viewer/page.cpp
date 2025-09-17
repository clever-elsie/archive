#include <crow/json.h>

#include <app/viewer/global.hpp>
#include <app/viewer/page.hpp>
#include <app/viewer/inline_helper.hpp>

namespace VIEWER{
using namespace std;

crow::json::wvalue get_page_list(const crow::request&req){
	lock_guard<mutex> lock(imtex);
	crow::json::wvalue ret;
	uint64_t n=leaf_dirs.size();
	ret["cnt"]= (n+Info_page_size-1)/Info_page_size;
	return ret;
}

crow::json::wvalue get_page(const crow::request&req){
	lock_guard<mutex> lock(imtex);
	auto data = crow::json::load(req.body);
	int64_t idx = data["idx"].i();
	crow::json::wvalue::list ret;
	uint64_t n=leaf_dirs.size();
	uint64_t page_cnt=(n+Info_page_size-1)/Info_page_size;
	if(0<=idx && idx<(int64_t)page_cnt){
		uint64_t start=(uint64_t)idx*Info_page_size;
		uint64_t end=min(n,start+Info_page_size);
		for(uint64_t k=start;k<end;++k){
			auto it=leaf_dirs.find_by_order(k);
			if(it!=leaf_dirs.end()) pb_next(ret,**it);
		}
	}
	return crow::json::wvalue(ret);
}

} // namespace VIEWER