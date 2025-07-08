#pragma once
#include "../../headers.hpp"
#include "../../manager/auth/middleware.hpp"
#include "../../manager/auth/auth.hpp"
#include "../../manager/users/user_manager.hpp"
#include <ranges>

namespace VIEWER{
using namespace std;
struct Info;

// グローバル変数
inline string base_dir;
inline string rel_base;
inline Info*root_dir = nullptr;
inline vector<Info*>leaf_dirs;
inline vector<Info*>dirs_tree;
inline constexpr uint64_t Info_page_size=9;
inline vector<array<Info*,Info_page_size>>pages;
inline mutex imtex;
inline random_device rds;
inline mt19937_64 R(rds());
inline filesystem::file_time_type base_time{};

struct Info{
	string path;
	set<string>author;
	set<string>tag;
	vector<Info*>dirs;
	vector<string>imgs;
	filesystem::file_time_type img_time;
	uint64_t id;
	Info*par;
	bool has_only_img;
	
	inline Info(const string&dir,Info*par_)
	:path(dir),author(),tag(),
	dirs(),imgs(),img_time(base_time),id(UINT64_MAX),par(par_),has_only_img(0){
		if(!filesystem::is_directory(dir))return;
		const string info=dir+"/.info";
		if(filesystem::exists(info)){
			ifstream ifs(info);
			string buf;
			while(getline(ifs,buf)){
				if(buf.size()<1)continue;
				if(buf.back()=='\n')buf.pop_back();
				const char F=buf.back();
				if(F!='T'&&F!='A')continue;
				buf.pop_back();
				if(F=='T') tag.emplace(buf);
				else author.emplace(buf);
			}
		}
		constexpr static array<string,5> exts{".webp",".jpg",".jpeg",".png",".gif"};
		constexpr static array<string,6> not_img{".mp4",".mp3",".flac",".aac",".wav",".txt"};
		for(const auto&itr:filesystem::directory_iterator(dir))
			if(itr.is_directory()){
				Info *n=new Info(itr.path().string(),this);
				if(n!=nullptr&&(n->imgs.size()||n->dirs.size())){
					dirs.push_back(n);
					if(n->dirs.size()){
						n->id=dirs_tree.size();
						dirs_tree.push_back(n);
					}else{
						n->id=leaf_dirs.size();
						leaf_dirs.push_back(n);
					}
				}else delete n;
			}else{
				for(const auto&ext:exts)
					if(const auto s=itr.path().filename().string();s.ends_with(ext)){
						imgs.emplace_back(s);
						break;
					}
				for(const auto&ext:not_img)
					if(const auto s=itr.path().string();s.ends_with(ext)){
						Info *n=new Info(s,this);
						dirs.push_back(n);
						break;
					}
			}
			if(imgs.size()){
				sort(imgs.begin(),imgs.end());
				img_time=filesystem::last_write_time(path+"/"+filesystem::path(imgs[0]).filename().string());
			}
			if(dirs.size()){
				sort(dirs.begin(),dirs.end(),[](const Info*a,const Info*b){
					bool c1=filesystem::is_directory(a->path);
					bool c2=filesystem::is_directory(b->path);
					if(c1==c2) return a->path<b->path;
					return c1;
				});
			}
			has_only_img=!dirs.size();
		}
};

inline void make_page_list(){
	pages.clear();
	pages.resize(1);
	pages[0].fill(nullptr);
	uint64_t count=0;
	vector<Info*>cp(leaf_dirs);
	sort(cp.begin(),cp.end(),[](const Info*a,const Info*b){
		if(a->img_time==b->img_time)
			return a->path<b->path;
		return a->img_time>b->img_time;
	});
	for(auto const & dir:cp){
		pages.back()[count++]=dir;
		if(Info_page_size==count){
			count=0;
			pages.push_back({});
		}
	}
	if(count==0) pages.pop_back();
}

inline void load_leaf_dir(const string&base){
	namespace C = std::chrono;
	namespace F = std::filesystem;
	leaf_dirs.clear();
	dirs_tree.clear();
	delete root_dir;
	dirs_tree.resize(1);
	root_dir=new Info(base,nullptr);
	dirs_tree[0]=root_dir;
	root_dir->par=root_dir;
	root_dir->id=0;
	make_page_list();
}

inline string rel_join(const string&dir){
	size_t start=0;
	while(dir[start]=='.'||dir[start]=='/')start++;
	return rel_base+string(dir.begin()+start,dir.end());
}

inline crow::response reload_leaf(const crow::request&req){
	// 管理者権限チェック
	string session_id = MIDDLEWARE::extract_session_id(req);
	if (session_id.empty() || !AUTH::validate_session(session_id)) {
		crow::json::wvalue error_response;
		error_response["error"] = "認証が必要です";
		return crow::response(401, error_response);
	}
	
	string username = AUTH::get_username_from_session(session_id);
	if (!USER_MANAGER::user_manager.is_admin(username)) {
		crow::json::wvalue error_response;
		error_response["error"] = "管理者権限が必要です";
		return crow::response(403, error_response);
	}
	
	lock_guard<mutex> lock(imtex);
	load_leaf_dir(base_dir);
	if(leaf_dirs.size()==0) return crow::response(400);
	return crow::response(200);
}

inline void pb_next(crow::json::wvalue::list&ret,const Info&info){
	if(info.imgs.size()){
		crow::json::wvalue next;
		next["img"]=filesystem::relative(filesystem::path(info.path)/info.imgs[0],rel_base);
		next["id"]=info.id;
		ret.push_back(next);
	}
}

inline vector<Info*> get_rand_dirs(const vector<Info*>&dirs,const int cnt){
	set<Info*>seen;
	while(seen.size()<cnt) seen.insert(dirs[R()%dirs.size()]);
	return vector(seen.begin(),seen.end());
}

inline crow::json::wvalue get_rand_imgs(){
	lock_guard<mutex> lock(imtex);
	constexpr int cnt=9;
	crow::json::wvalue::list ret;
	for(auto&dir:get_rand_dirs(leaf_dirs,cnt))
		pb_next(ret,*dir);
	return crow::json::wvalue(ret);
}

inline crow::json::wvalue get_imgs(const crow::request&req){
	lock_guard<mutex> lock(imtex);
	auto data = crow::json::load(req.body);
	int64_t id = data["id"].i();
	const std::string bpath(leaf_dirs[id]->path);
	vector<string>& imgs=leaf_dirs[id]->imgs;
	crow::json::wvalue::list img_list;
	crow::json::wvalue ret;
	for(auto const & img:imgs){
		crow::json::wvalue next;
		next["img"]=filesystem::relative(filesystem::path(bpath)/img,rel_base);
		img_list.push_back(move(next));
	}
	ret["img"]=move(img_list);
	const string info=leaf_dirs[id]->path+"/.info";
	if(!filesystem::exists(info))
		ofstream ofs(info);
	crow::json::wvalue::list as,ts;
	ret["id"]=id;
	for(const auto&x:leaf_dirs[id]->author)
		as.push_back(html_escape(x));
	ret["creators"]=crow::json::wvalue(as);
	for(const auto&x:leaf_dirs[id]->tag)
		ts.push_back(html_escape(x));
	ret["tags"]=crow::json::wvalue(ts);
	return crow::json::wvalue(ret);
}

inline crow::json::wvalue get_creators_all(const crow::request&req){
	lock_guard<mutex> lock(imtex);
	auto data = crow::json::load(req.body);
	int64_t id = data["id"].i();
	Info*creator=leaf_dirs[id]->par;
	crow::json::wvalue::list ret;
	for(auto const &dir:creator->dirs) pb_next(ret,*dir);
	return crow::json::wvalue(ret);
}

// return not views_size, but offset to next token head
inline pair<size_t,string> get_token(size_t idx,const string&s){
	size_t cnt=0;
	string ret;
	bool token=true;
	for(const auto&x:s|views::drop(idx)){
		if(token){
			if(x==' ') token=false;
			else if(x=='('||x==')'||x=='!')break;
			else ret.push_back(x);
		}else if(x!=' ')break;
		++cnt;
	}
	return{cnt,ret};
}

template<bool or_exp=false>
inline bool parse_query(size_t&idx,const Info&tar,const string&s){
	bool nx_not=false;
	while(idx<s.size()){
		if(s[idx]=='!'){
			nx_not=!nx_not;
			++idx;
			continue;
		}
		bool r;
		switch(s[idx]){
			case '(': r=parse_query<!or_exp>(++idx,tar,s); break;
			case ')': ++idx;return !or_exp;
			default:{
				auto[cnt,token]=get_token(idx,s);
				idx+=cnt;
				r=tar.author.contains(token)
				||tar.tag.contains(token)
				||tar.path.contains(token);
			}break;
		}
		if(nx_not) r=!r;
		if constexpr(or_exp){
			if(r) return true;
		}else{ if(!r) return false; }
	}
	return !or_exp;
}

inline crow::json::wvalue retrieve_query(const crow::request& req){
	lock_guard<mutex> lock(imtex);
	const string querys = crow::json::load(req.body).s();
	crow::json::wvalue::list ret;
	vector<Info*>dirs;
	for(auto const &dir:leaf_dirs)
		if(size_t idx=0;parse_query(idx,*dir,querys))
			dirs.push_back(dir);
	sort(dirs.begin(),dirs.end(),[](const Info*a,const Info*b){
		return a->path<b->path;
	});
	for(auto const &dir:dirs) pb_next(ret,*dir);
	return ret;
}

inline crow::json::wvalue get_page_list(const crow::request&req){
	lock_guard<mutex> lock(imtex);
	crow::json::wvalue ret;
	ret["cnt"]=pages.size();
	return ret;
}

inline crow::json::wvalue get_page(const crow::request&req){
	lock_guard<mutex> lock(imtex);
	auto data = crow::json::load(req.body);
	int64_t idx = data["idx"].i();
	crow::json::wvalue::list ret;
	if(0<=idx&&idx<pages.size()){
		for(auto const &dir:pages[idx])
			if(dir) pb_next(ret,*dir);
	}
	return crow::json::wvalue(ret);
}

inline crow::json::wvalue get_dir_list(const crow::request&req){
	lock_guard<mutex> lock(imtex);
	namespace F = std::filesystem;
	auto data = crow::json::load(req.body);
	const int64_t id=data["id"].i();
	Info* tar=dirs_tree[id];
	crow::json::wvalue ret;
	ret["cur"]=id;
	ret["par"]=tar->par->id;

	crow::json::wvalue::list dir,img;
	for(const auto&d:tar->dirs){
		if(d->has_only_img) pb_next(img,*d);
		else{
			crow::json::wvalue next;
			next["path"]=(d->path);
			next["dirname"]=(filesystem::path(d->path).filename());
			next["id"]=d->id;
			dir.emplace_back(move(next));
		}
	}
	ret["imgs"]=crow::json::wvalue(move(img));
	ret["dirs"]=crow::json::wvalue(move(dir));
	return ret;
}

inline crow::response info_renew(const crow::request&req){
	// 管理者権限チェック
	string session_id = MIDDLEWARE::extract_session_id(req);
	if (session_id.empty() || !AUTH::validate_session(session_id)) {
		crow::json::wvalue error_response;
		error_response["error"] = "認証が必要です";
		return crow::response(401, error_response);
	}
	
	string username = AUTH::get_username_from_session(session_id);
	if (!USER_MANAGER::user_manager.is_admin(username)) {
		crow::json::wvalue error_response;
		error_response["error"] = "管理者権限が必要です";
		return crow::response(403, error_response);
	}
	
	lock_guard<mutex> lock(imtex);
	const auto data=crow::json::load(req.body);
	int64_t id=data["id"].i();
	string tar=data["data"].s();
	string info=leaf_dirs[id]->path+"/.info";
	if(data["AD"].s()=="A"){
		string buf;
		ifstream ifs(info);
		bool already_has=false;
		while(getline(ifs,buf)){
			if(buf.size()<1)continue;
			if(buf.back()=='\n')buf.pop_back();
			if(buf==tar){
				already_has=true;
				break;
			}
		}
		ifs.close();
		if(!already_has){
			ofstream ofs(info,ios_base::app);
			ofs<<tar<<'\n';
			char F=tar.back();
			tar.pop_back();
			if(F=='A') leaf_dirs[id]->author.emplace(move(tar));
			else leaf_dirs[id]->tag.emplace(move(tar));
		}
	}else{ // delete
		vector<string>dbuf;
		string buf;
		ifstream ifs(info);
		while(getline(ifs,buf)){
			if(buf.size()<1)continue;
			if(buf.back()=='\n')buf.pop_back();
			if(buf!=tar) dbuf.emplace_back(move(buf));
		}
		ifs.close();
		ofstream ofs(info,ios_base::trunc);
		for(const auto&x:dbuf)
			ofs<<x<<'\n';
		char F=tar.back();
		tar.pop_back();
		if(F=='A')leaf_dirs[id]->author.erase(tar);
		else leaf_dirs[id]->tag.erase(tar);
	}
	return crow::response(200);
}

}// namespace comic