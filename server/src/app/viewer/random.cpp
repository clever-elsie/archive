#include <app/viewer/random.hpp>
#include <app/viewer/inline_helper.hpp>

namespace VIEWER{
using namespace std;

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

vector<Info*> get_rand_dirs(const int cnt, TreeType filter_type){
	set<Info*>seen;
	manager&mgr = manager::get_instance();
	const auto& target_tree = mgr.trackable_trees[static_cast<size_t>(filter_type)];
	const int n = std::min(cnt, static_cast<int>(target_tree.size()));
	if (n <= 0) return {};
	while(seen.size()<n){
		auto k=mgr.R()%target_tree.size();
		auto it=target_tree.find_by_order(k);
		if(it==target_tree.end()) break;
		seen.insert(*it);
	}
	return vector(seen.begin(),seen.end());
}

crow::json::wvalue get_rand_imgs(const crow::request& req, int cnt){
	constexpr int max_limit=24;
	if(cnt<0) return crow::json::wvalue();
	if(cnt>max_limit) cnt=max_limit;
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	crow::json::wvalue::list ret;
	
	const char* filter_c = req.url_params.get("filter");
	TreeType filter_type = parse_filter(filter_c);
	
	for(auto&dir:get_rand_dirs(cnt, filter_type))
		pb_next(ret, dir);
	return crow::json::wvalue(ret);
}

} // namespace VIEWER