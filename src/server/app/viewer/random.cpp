#include <app/viewer/random.hpp>
#include <app/viewer/inline_helper.hpp>

namespace VIEWER{
using namespace std;

vector<Info*> get_rand_dirs(const int cnt){
	set<Info*>seen;
	if(leaf_dirs.size()==0) return {};
	while(seen.size()<static_cast<size_t>(cnt)){
		auto k=R()%leaf_dirs.size();
		auto it=leaf_dirs.find_by_order(k);
		if(it==leaf_dirs.end()) break;
		seen.insert(*it);
	}
	return vector(seen.begin(),seen.end());
}

crow::json::wvalue get_rand_imgs(int cnt){
	constexpr int max_limit=24;
	if(cnt<0) return crow::json::wvalue();
	if(cnt>max_limit) cnt=max_limit;
	lock_guard<mutex> lock(imtex);
	crow::json::wvalue::list ret;
	for(auto&dir:get_rand_dirs(cnt))
		pb_next(ret,*dir);
	return crow::json::wvalue(ret);
}

} // namespace VIEWER