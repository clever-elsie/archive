#pragma once
#include "../../headers.hpp"

namespace MEMO{
using namespace std;
using i64=int64_t;
using u64=uint64_t;

class mex{
	map<i64,i64>v; // [l,r)
	unordered_map<i64,i64>c; // {val,cnt}
	public:
	mex(){
		v[-1]=0;
		c[-1]=1;
	}
	~mex()=default;
	void add(i64 a){
		{ // count phase
			auto itr=c.find(a);
			if(itr!=c.end()){
				itr->second++;
				return;
			}else c[a]=1;
		}
		if(a<0)return;
		auto itr=v.upper_bound(a);
		itr--;
		if(a<itr->second)return;
		if(itr->second==a) itr->second++;
		else{
			v[a]=a+1;
			itr=v.find(a);
		}
		auto jtr=itr;
		jtr++;
		if(jtr==v.end())return;
		if(itr->second==jtr->first){
			itr->second=jtr->second;
			v.erase(jtr);
		}
	}
	void erase(i64 a){
		{ // count phase
			auto itr=c.find(a);
			if(itr==c.end())return;
			itr->second--;
			if(itr->second>0) return;
			c.erase(itr);
		}
		if(a<0)return;
		auto itr=v.upper_bound(a);
		itr--;
		if(itr->second-1==a){
			itr->second--;
			return;
		}
		v[a+1]=itr->second;
		itr->second=a;
	}
	i64 find(){return v[-1];}
	size_t count(i64 a){
		auto itr=c.find(a);
		return(itr==c.end()?0ull:itr->second);
	}
};

// グローバル変数
inline string buf_path;
inline map<i64,string>memo;
inline mutex mmtex;
inline set<i64>Ngen;

inline crow::json::wvalue memo_fetch_all(const crow::request &req) {
	lock_guard<mutex> lock(mmtex);
	if(memo.size()==0&&filesystem::exists(buf_path)){
		for(const auto &file:filesystem::directory_iterator(buf_path)){
			filesystem::path path=file.path();
			if(ifstream ifs(path);ifs){
				string dat{istreambuf_iterator<char>(ifs),istreambuf_iterator<char>()};
				i64 id=stol(path.filename().string());
				memo.emplace(id,dat);
			}
		}
	}
	crow::json::wvalue::list v;
	for (const auto &m : memo) {
		crow::json::wvalue x;
		x["id"] = m.first;
		x["memo"] = m.second;
		v.push_back(std::move(x));
	}
	return crow::json::wvalue(std::move(v));
}

inline crow::json::wvalue memo_issue_new_id(const crow::request &req) {
	lock_guard<mutex> lock(mmtex);
	crow::json::wvalue ret;
	i64 id = 1;
	while(Ngen.find(id) != Ngen.end()) {
		id++;
	}
	Ngen.insert(id);
	ret["id"]=id;
	memo[id]="";
	return ret;
}

inline crow::response memo_renew(const crow::request &req) {
	lock_guard<mutex> lock(mmtex);
	auto data = crow::json::load(req.body);
	i64 id = stol(string(data["id"].s()));
	string new_data = data["memo"].s();
	if(Ngen.find(id)==Ngen.end())
		Ngen.insert(id);
	memo[id] = new_data;
	ofstream ofs(buf_path+to_string(id),ios_base::trunc);
	ofs<<new_data<<endl;
	return crow::response(200);
}

inline crow::response memo_now(const crow::request&req){
	lock_guard<mutex> lock(mmtex);
	auto data = crow::json::load(req.body);
	i64 id = stol(string(data["id"].s()));
	crow::json::wvalue ret;
	if (memo.find(id) == memo.end())
		return crow::response(400);
	ret["memo"] = memo[id];
	return crow::response(ret);
}

inline crow::response memo_rm(const crow::request &req) {
	lock_guard<mutex> lock(mmtex);
	auto data = crow::json::load(req.body);
	i64 id=stol(string(data["id"].s()));
	if(memo.find(id)!=memo.end()){
		memo.erase(id);
		Ngen.erase(id);
		filesystem::path file(buf_path+to_string(id));
		if(filesystem::exists(file))
			filesystem::remove(file);
	}else return crow::response(400);
	return crow::response(200);
}

}//namespace MEMO