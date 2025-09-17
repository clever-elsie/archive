#include <algorithm>
#include <ranges>

#include <app/retrieve.hpp>
#include <app/viewer/retrieve.hpp>
#include <app/viewer/inline_helper.hpp>

namespace VIEWER{
using namespace std;

crow::json::wvalue retrieve_query(const crow::request& req){
	lock_guard<mutex> lock(imtex);
	const string querys = crow::json::load(req.body).s();
	crow::json::wvalue::list ret;
	vector<Info*>dirs;
	for(auto it=leaf_dirs.begin();it!=leaf_dirs.end();++it)
		if(size_t idx=0;RETRIEVE::parse_query(idx,**it,querys))
			dirs.push_back(*it);
	ranges::sort(dirs,[](const Info*a,const Info*b){
		return a->path<b->path;
	});
	for(auto const &dir:dirs) pb_next(ret,*dir);
	return ret;
}

} // namespace VIEWER