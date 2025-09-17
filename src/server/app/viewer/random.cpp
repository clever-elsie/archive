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

crow::json::wvalue get_rand_imgs(){
	lock_guard<mutex> lock(imtex);
	constexpr int cnt=Info_page_size;
	crow::json::wvalue::list ret;
	for(auto&dir:get_rand_dirs(cnt))
		pb_next(ret,*dir);
	return crow::json::wvalue(ret);
}

} // namespace VIEWER