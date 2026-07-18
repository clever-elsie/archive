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

crow::response retrieve_query(const crow::request& req){
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	const char* query_c = req.url_params.get("query");
	const char* order_key_c = req.url_params.get("order_key");
	const char* order_c = req.url_params.get("order");
	const char* filter_c = req.url_params.get("filter");
	if(!query_c) return crow::response(400);

	auto queryAST = RETRIEVE::parse_query(std::string(query_c));
	if(!queryAST) return crow::response(400);
	for(const auto&line:queryAST->to_string()){
		std::cout<<line<<'\n';
	}
	
	TreeType filter_type = parse_filter(filter_c);
	const auto& target_tree = mgr.trackable_trees[static_cast<size_t>(filter_type)];
	
	using MatchedItem = std::variant<Info*, VideoFile*>;
	vector<MatchedItem> matched_items;

	if(VIEWER::is_admin_req(req)){
		for(auto it=target_tree.begin();it!=target_tree.end();++it)
			if(queryAST->evaluate(**it))
				matched_items.push_back(*it);
	}else{
		for(auto it=target_tree.begin();it!=target_tree.end();++it)
			if(queryAST->evaluate(**it))
				if(VIEWER::can_view_node(req, *it))
					matched_items.push_back(*it);
	}

	if (filter_type == TreeType::all || filter_type == TreeType::movies) {
		if(VIEWER::is_admin_req(req)){
			for(auto it=mgr.video_tree.begin();it!=mgr.video_tree.end();++it)
				if(queryAST->evaluate(**it))
					matched_items.push_back(*it);
		}else{
			for(auto it=mgr.video_tree.begin();it!=mgr.video_tree.end();++it)
				if(queryAST->evaluate(**it))
					if(VIEWER::can_view_node(req, (*it)->parent_node))
						matched_items.push_back(*it);
		}
	}

	Info::SortingOrder order = (order_key_c && std::string(order_key_c) == "last_write_time") ? Info::SortingOrder::last_write_time : Info::SortingOrder::name;
	bool descendant = order_c && std::string(order_c) == "descendant";

	auto get_time = [](const MatchedItem& item) -> std::filesystem::file_time_type {
		if (std::holds_alternative<Info*>(item)) {
			return std::get<Info*>(item)->last_write_time_value();
		} else {
			return std::get<VideoFile*>(item)->last_write_time;
		}
	};

	auto get_name = [](const MatchedItem& item) -> std::string {
		if (std::holds_alternative<Info*>(item)) {
			return std::get<Info*>(item)->sortkey_value();
		} else {
			return std::get<VideoFile*>(item)->name;
		}
	};

	std::sort(matched_items.begin(), matched_items.end(), [&](const MatchedItem& a, const MatchedItem& b) {
		auto ta = get_time(a);
		auto tb = get_time(b);
		bool is_less = false;
		if (order == Info::SortingOrder::last_write_time) {
			if (ta != tb) {
				is_less = ta < tb;
			} else {
				is_less = get_name(a) < get_name(b);
			}
		} else {
			auto na = get_name(a);
			auto nb = get_name(b);
			if (na != nb) {
				is_less = na < nb;
			} else {
				is_less = ta < tb;
			}
		}
		return descendant ? !is_less : is_less;
	});

	crow::json::wvalue::list ret;
	for(auto const &item : matched_items) {
		if (std::holds_alternative<Info*>(item)) {
			auto dir = std::get<Info*>(item);
			std::error_code ec;
			if (!std::filesystem::exists(dir->full_path(), ec) || ec) {
				continue;
			}
			try {
				pb_next(ret, dir);
			} catch(...) {}
		} else {
			auto vf = std::get<VideoFile*>(item);
			std::error_code ec;
			if (vf->parent_node && (!std::filesystem::exists(vf->parent_node->full_path() / vf->path, ec) || ec)) {
				continue;
			}
			try {
				pb_next(ret, vf);
			} catch(...) {}
		}
	}
	return crow::response(200,crow::json::wvalue(move(ret)));
}

} // namespace VIEWER
