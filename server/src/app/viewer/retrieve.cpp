#include <algorithm>
#include <ranges>
#include <memory>

#include <app/retrieve.hpp>
#include <app/viewer/retrieve.hpp>
#include <app/viewer/inline_helper.hpp>
#include <app/viewer/manager.hpp>
#include <app/viewer/access_control.hpp>

namespace VIEWER{
using namespace std;

crow::response retrieve_query(const crow::request& req){
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	const char* query_c = req.url_params.get("query");
	const char* order_key_c = req.url_params.get("order_key");
	const char* order_c = req.url_params.get("order");
	if(!query_c) return crow::response(400);

	auto queryAST = RETRIEVE::parse_query(std::string(query_c));
	if(!queryAST) return crow::response(400);
	for(const auto&line:queryAST->to_string()){
		std::cout<<line<<'\n';
	}
	vector<Info*>dirs;
	if(VIEWER::is_admin_req(req)){
		for(auto it=mgr.leaf_dirs.begin();it!=mgr.leaf_dirs.end();++it)
			if(queryAST->evaluate(**it))
				dirs.push_back(*it);
	}else{
		for(auto it=mgr.leaf_dirs.begin();it!=mgr.leaf_dirs.end();++it)
			if(queryAST->evaluate(**it))
				if(VIEWER::can_view_node(req, *it))
					dirs.push_back(*it);
	}
	Info::SortingOrder order = (order_key_c && std::string(order_key_c) == "last_write_time") ? Info::SortingOrder::last_write_time : Info::SortingOrder::name;
	bool descendant = order_c && std::string(order_c) == "descendant";
	Info::sort(dirs, order, descendant);
	crow::json::wvalue::list ret;
	for(auto const &dir:dirs) pb_next(ret,dir->current_thumbnail_relative_path(), dir->id());
	return crow::response(200,crow::json::wvalue(move(ret)));
}

} // namespace VIEWER