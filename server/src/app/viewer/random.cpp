#include <app/viewer/random.hpp>
#include <app/viewer/inline_helper.hpp>

namespace VIEWER{
using namespace std;

vector<Info*> get_rand_dirs(const int cnt){
	set<Info*>seen;
	manager&mgr = manager::get_instance();
	const int n = std::min(cnt, static_cast<int>(mgr.leaf_dirs.size()));
	while(seen.size()<n){
		auto k=mgr.R()%mgr.leaf_dirs.size();
		auto it=mgr.leaf_dirs.find_by_order(k);
		if(it==mgr.leaf_dirs.end()) break;
		if((*it)->refresh(0)) seen.insert(*it);
	}
	return vector(seen.begin(),seen.end());
}

crow::json::wvalue get_rand_imgs(int cnt){
	constexpr int max_limit=24;
	if(cnt<0) return crow::json::wvalue();
	if(cnt>max_limit) cnt=max_limit;
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	crow::json::wvalue::list ret;
	for(auto&dir:get_rand_dirs(cnt))
		pb_next(ret,dir->current_thumbnail_relative_path(), dir->id());
	return crow::json::wvalue(ret);
}

} // namespace VIEWER