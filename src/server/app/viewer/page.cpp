#include <crow/json.h>

#include <app/viewer/page.hpp>
#include <app/viewer/inline_helper.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER{
using namespace std;

crow::json::wvalue get_page_list(const crow::request&req){
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	crow::json::wvalue ret;
	uint64_t n=mgr.leaf_dirs.size();
	ret["cnt"]= (n+mgr.Info_page_size-1)/mgr.Info_page_size;
	return ret;
}

crow::json::wvalue get_page(const crow::request&req){
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	auto data = crow::json::load(req.body);
	int64_t idx = data["idx"].i();
	crow::json::wvalue::list ret;
	uint64_t n=mgr.leaf_dirs.size();
	uint64_t page_cnt=(n+mgr.Info_page_size-1)/mgr.Info_page_size;
	if(0<=idx && idx<(int64_t)page_cnt){
		uint64_t start=(uint64_t)idx*mgr.Info_page_size;
		uint64_t end=min(n,start+mgr.Info_page_size);
		for(uint64_t k=start;k<end;++k){
			auto it=mgr.leaf_dirs.find_by_order(k);
			if(it!=mgr.leaf_dirs.end())
				if((*it)->refresh(0))
					pb_next(ret,**it);
				else --k;
		}
	}
	return crow::json::wvalue(ret);
}

} // namespace VIEWER