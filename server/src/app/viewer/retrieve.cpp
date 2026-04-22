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
	const crow::json::rvalue json = crow::json::load(req.body);
	auto queryAST = RETRIEVE::parse_query(json["query"].s());
	if(!queryAST) return crow::response(400);
	for(const auto&line:queryAST->to_string()){
		std::cout<<line<<'\n';
	}
	vector<Info*>dirs;
	if(VIEWER::is_admin_req(req)){
		for(auto it=mgr.leaf_dirs.begin();it!=mgr.leaf_dirs.end();++it)
			if((*it)->refresh(0) && queryAST->evaluate(**it))
				dirs.push_back(*it);
	}else{
		for(auto it=mgr.leaf_dirs.begin();it!=mgr.leaf_dirs.end();++it)
			if((*it)->refresh(0) && queryAST->evaluate(**it))
				if(VIEWER::can_view_node(req, *it))
					dirs.push_back(*it);
	}
	Info::SortingOrder order = json["order_key"].s()=="last_write_time"?Info::SortingOrder::last_write_time:Info::SortingOrder::name;
	bool descendant = json["order"].s()=="descendant";
	Info::sort(dirs, order, descendant);
	crow::json::wvalue::list ret;
	for(auto const &dir:dirs) pb_next(ret,dir->current_thumbnail_relative_path(), dir->id());
	return crow::response(200,crow::json::wvalue(move(ret)));
}

} // namespace VIEWER