#pragma once
#include <ext/pb_ds/tree_policy.hpp>
#include <ext/pb_ds/assoc_container.hpp>
#include <unordered_set>
#include "manager/auth/middleware.hpp"
#include "manager/auth/auth.hpp"
#include "manager/users/user_manager.hpp"
#include "app/retrieve.hpp"
#include <fstream>
#include <crow/multipart.h>
#include "inline_helper.hpp"

namespace VIEWER{
template<class key, class value, class cmp=std::less<key>>
using tree=__gnu_pbds::tree<key, value, cmp, __gnu_pbds::rb_tree_tag, __gnu_pbds::tree_order_statistics_node_update>;
using namespace std;
struct Info;
// PBDS 比較子（実装は後方）
struct LeafCmp{ static bool operator()(const Info* a,const Info* b) noexcept; };
struct DirCmp { static bool operator()(const Info* a,const Info* b) noexcept; };

// グローバル変数
inline string base_dir;
inline string rel_base;
inline Info*root_dir = nullptr;
// 可乱択二分木（順序統計木）に変更
inline tree<Info*, __gnu_pbds::null_type, LeafCmp> leaf_dirs;
inline tree<Info*, __gnu_pbds::null_type, DirCmp> dirs_tree;
inline constexpr uint64_t Info_page_size=12;
inline mutex imtex;
inline random_device rds;
inline mt19937_64 R(rds());
inline filesystem::file_time_type base_time{};
inline unordered_set<Info*> valid_info_ptrs; // 有効ポインタ集合

// id(数値) → Info* 変換（0 は root_dir のエイリアス）
inline Info* get_info_from_id(uint64_t idv) noexcept{
	return idv==0 ? root_dir : reinterpret_cast<Info*>(idv);
}

struct Info{
	string path;
	set<string>tag;
	vector<Info*>dirs;
	vector<string>imgs;
	filesystem::file_time_type last_write_time;
	uint64_t id;
	Info*par;
	bool has_only_img;
	
	inline Info(const string&dir,Info*par_)
	:path(dir),tag(),
	dirs(),imgs(),id(UINT64_MAX),par(par_),has_only_img(0){
		last_write_time=filesystem::last_write_time(dir);
		if(!filesystem::is_directory(dir))return;
		const string info=dir+"/.info";
		if(filesystem::exists(info)){
			ifstream ifs(info);
			string buf;
			while(getline(ifs,buf)){
				if(buf.size()<1)continue;
				if(buf.back()=='\n')buf.pop_back();
				tag.emplace(buf);
			}
		}
		constexpr static array<string,5> exts{".webp",".jpg",".jpeg",".png",".gif"};
		constexpr static array<string,6> not_img{".mp4",".mp3",".flac",".aac",".wav",".txt"};
		for(const auto&itr:filesystem::directory_iterator(dir))
			if(itr.is_directory()){
				Info *n=new Info(itr.path().string(),this);
				if(n!=nullptr&&(n->imgs.size()||n->dirs.size())){
					dirs.push_back(n);
					n->id=reinterpret_cast<uint64_t>(n);
					valid_info_ptrs.insert(n);
					if(n->dirs.size()) dirs_tree.insert(n);
					else leaf_dirs.insert(n);
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
			if(imgs.size()) last_write_time=filesystem::last_write_time(filesystem::path(path)/imgs[0]);
			sort(imgs.begin(),imgs.end());
			sort(dirs.begin(),dirs.end(),[](const Info*a,const Info*b){
				bool c1=filesystem::is_directory(a->path);
				bool c2=filesystem::is_directory(b->path);
				if(c1==c2) return a->path<b->path;
				return c1;
			});
			has_only_img=!dirs.size();
		}
};

inline void load_leaf_dir(const string&base){
	namespace C = std::chrono;
	namespace F = std::filesystem;
	leaf_dirs.clear();
	dirs_tree.clear();
	valid_info_ptrs.clear();
	delete root_dir;
	root_dir=new Info(base,nullptr);
	root_dir->par=root_dir;
	root_dir->id=reinterpret_cast<uint64_t>(root_dir);
	valid_info_ptrs.insert(root_dir);
	dirs_tree.insert(root_dir);
}

inline string rel_join(const string&dir){
	size_t start=0;
	while(dir[start]=='.'||dir[start]=='/')start++;
	return rel_base+string(dir.begin()+start,dir.end());
}

inline crow::response reload_leaf(const crow::request&req){
	// 管理者権限チェック
	string token = MIDDLEWARE::extract_token(req);
	if (token.empty() || !AUTH::validate_token_wrapper(token)) {
		crow::json::wvalue error_response;
		error_response["error"] = "認証が必要です";
		return crow::response(401, error_response);
	}
	
	string username = AUTH::get_username_from_token(token);
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
inline bool LeafCmp::operator()(const Info* a,const Info* b) noexcept{
	if(a==b) return false;
	if(a->last_write_time!=b->last_write_time) return a->last_write_time>b->last_write_time;
	if(a->path!=b->path) return a->path<b->path;
	return a<b;
}
inline bool DirCmp::operator()(const Info* a,const Info* b) noexcept{
	if(a==b) return false;
	if(a->path!=b->path) return a->path<b->path;
	return a<b;
}

inline vector<Info*> get_rand_dirs(const int cnt){
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

inline crow::json::wvalue get_rand_imgs(){
	lock_guard<mutex> lock(imtex);
	constexpr int cnt=Info_page_size;
	crow::json::wvalue::list ret;
	for(auto&dir:get_rand_dirs(cnt))
		pb_next(ret,*dir);
	return crow::json::wvalue(ret);
}

inline crow::json::wvalue get_imgs(const crow::request&req){
	lock_guard<mutex> lock(imtex);
	auto data = crow::json::load(req.body);
	uint64_t idv = static_cast<uint64_t>(data["id"].i());
	Info* node = get_info_from_id(idv);
	if(!node || !valid_info_ptrs.contains(node) || !node->has_only_img) return crow::json::wvalue();
	const std::string bpath(node->path);
	vector<string>& imgs=node->imgs;
	crow::json::wvalue::list img_list;
	crow::json::wvalue ret;
	for(auto const & img:imgs){
		crow::json::wvalue next;
		next["img"]=filesystem::relative(filesystem::path(bpath)/img,rel_base);
		img_list.push_back(move(next));
	}
	ret["img"]=move(img_list);
	const string info=node->path+"/.info";
	if(!filesystem::exists(info))
		ofstream ofs(info);
	crow::json::wvalue::list ts;
	ret["id"]=idv;
	for(const auto&x:node->tag)
		ts.push_back(html_escape(x)); // #include "inline_helper.hpp"
	ret["tags"]=crow::json::wvalue(ts);
	// 追加: 親ディレクトリの全画像ディレクトリサムネイル
	crow::json::wvalue::list parent_list;
	Info* parent = node->par;
	for(const auto &dir : parent->dirs) pb_next(parent_list, *dir);
	ret["parent"] = std::move(parent_list);
	return crow::json::wvalue(ret);
}

inline crow::json::wvalue retrieve_query(const crow::request& req){
	lock_guard<mutex> lock(imtex);
	const string querys = crow::json::load(req.body).s();
	crow::json::wvalue::list ret;
	vector<Info*>dirs;
	for(auto it=leaf_dirs.begin();it!=leaf_dirs.end();++it)
		if(size_t idx=0;RETRIEVE::parse_query(idx,**it,querys))
			dirs.push_back(*it);
	sort(dirs.begin(),dirs.end(),[](const Info*a,const Info*b){
		return a->path<b->path;
	});
	for(auto const &dir:dirs) pb_next(ret,*dir);
	return ret;
}

inline crow::json::wvalue get_page_list(const crow::request&req){
	lock_guard<mutex> lock(imtex);
	crow::json::wvalue ret;
	uint64_t n=leaf_dirs.size();
	ret["cnt"]= (n+Info_page_size-1)/Info_page_size;
	return ret;
}

inline crow::json::wvalue get_page(const crow::request&req){
	lock_guard<mutex> lock(imtex);
	auto data = crow::json::load(req.body);
	int64_t idx = data["idx"].i();
	crow::json::wvalue::list ret;
	uint64_t n=leaf_dirs.size();
	uint64_t page_cnt=(n+Info_page_size-1)/Info_page_size;
	if(0<=idx && idx<(int64_t)page_cnt){
		uint64_t start=(uint64_t)idx*Info_page_size;
		uint64_t end=min(n,start+Info_page_size);
		for(uint64_t k=start;k<end;++k){
			auto it=leaf_dirs.find_by_order(k);
			if(it!=leaf_dirs.end()) pb_next(ret,**it);
		}
	}
	return crow::json::wvalue(ret);
}

inline crow::json::wvalue get_dir_list(const crow::request&req){
	lock_guard<mutex> lock(imtex);
	namespace F = std::filesystem;
	auto data = crow::json::load(req.body);
	const uint64_t idv=static_cast<uint64_t>(data["id"].i());
	std::string order_key = data.has("order_key") ? data["order_key"].s() : std::string("name");
	std::string order = data.has("order") ? data["order"].s() : std::string("ascendant");
	Info* tar=get_info_from_id(idv);
	if(!tar || !valid_info_ptrs.contains(tar)) return crow::json::wvalue();
	crow::json::wvalue ret;
	ret["cur"]=idv;
	ret["par"]=tar->par->id;

	// vectorに詰め替え
	std::vector<Info*> dirvec,imgvec;
	for(const auto&d:tar->dirs)
		(d->has_only_img?imgvec:dirvec).push_back(d);
	// ソート 元々名前昇順
	if(order_key=="last_write_time"){
		auto cmp=[](const Info*a,const Info*b){
			if(a->last_write_time==b->last_write_time)
				return a->path<b->path;
			return a->last_write_time<b->last_write_time;
		};
		std::sort(dirvec.begin(), dirvec.end(), cmp);
		std::sort(imgvec.begin(), imgvec.end(), cmp);
	}
	if(order=="descendant"){
		std::reverse(dirvec.begin(), dirvec.end());
		std::reverse(imgvec.begin(), imgvec.end());
	}

	crow::json::wvalue::list dir,img;
	for(const auto&d:imgvec) pb_next(img,*d);
	for(const auto&d:dirvec){
		crow::json::wvalue next;
		next["path"]=(d->path);
		next["dirname"]=(filesystem::path(d->path).filename());
		next["id"]=d->id;
		dir.emplace_back(move(next));
	}
	ret["imgs"]=crow::json::wvalue(move(img));
	ret["dirs"]=crow::json::wvalue(move(dir));
	return ret;
}

inline crow::response info_renew(const crow::request&req){
	string token = MIDDLEWARE::extract_token(req);
	string username = AUTH::get_username_from_token(token);
	if (!USER_MANAGER::user_manager.is_admin(username)) {
		crow::json::wvalue error_response;
		error_response["error"] = "管理者権限が必要です";
		return crow::response(403, error_response);
	}
	lock_guard<mutex> lock(imtex);
	const auto data=crow::json::load(req.body);
	uint64_t idv=static_cast<uint64_t>(data["id"].i());
	string tar=data["data"].s();
	Info* node=get_info_from_id(idv);
	if(!node || !valid_info_ptrs.contains(node) || !node->has_only_img) return crow::response(404);
	string info=node->path+"/.info";
	if(data["AD"].s()=="add"){
		if(node->tag.contains(tar)) return crow::response(200);
		node->tag.emplace(move(tar));
		ofstream ofs(info,ios_base::app);
		ofs<<tar<<'\n';
	}else{ // delete
		node->tag.erase(tar);
		ofstream ofs(info,ios_base::trunc);
		for(const auto&x:node->tag)
			ofs<<x<<'\n';
	}
	return crow::response(200);
}

inline crow::response get_file_binary(const crow::request&req){
    // 認証不要（公開リソースとして）
    auto data = crow::json::load(req.body);
    if (!data) return crow::response(400, "Invalid JSON");
    std::string type = data["type"].s();
    uint64_t idv = static_cast<uint64_t>(data["id"].i());
    std::string filename = data["filename"].s();
    std::string fullpath;
    if(type=="image"||type=="video"||type=="audio"||type=="text"){
        Info* node=get_info_from_id(idv);
        if(!node || !valid_info_ptrs.contains(node)) return crow::response(404);
        if(type=="image" && !node->has_only_img) return crow::response(404);
        fullpath = node->path + "/" + filename;
    }else{
        return crow::response(400, "Unknown type");
    }
    std::ifstream ifs(fullpath, std::ios::binary);
    if(!ifs) return crow::response(404, "File not found");
    std::vector<char> buffer((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    crow::response res;
    // Content-Type判定
    if(type=="image"){
        if(filename.ends_with(".jpg")||filename.ends_with(".jpeg")) res.set_header("Content-Type","image/jpeg");
        else if(filename.ends_with(".png")) res.set_header("Content-Type","image/png");
        else if(filename.ends_with(".webp")) res.set_header("Content-Type","image/webp");
        else if(filename.ends_with(".gif")) res.set_header("Content-Type","image/gif");
        else res.set_header("Content-Type","application/octet-stream");
    }else if(type=="video"){
        if(filename.ends_with(".mp4")) res.set_header("Content-Type","video/mp4");
        else res.set_header("Content-Type","application/octet-stream");
    }else if(type=="audio"){
        if(filename.ends_with(".mp3")) res.set_header("Content-Type","audio/mpeg");
        else if(filename.ends_with(".flac")) res.set_header("Content-Type","audio/flac");
        else if(filename.ends_with(".aac")) res.set_header("Content-Type","audio/aac");
        else if(filename.ends_with(".wav")) res.set_header("Content-Type","audio/wav");
        else res.set_header("Content-Type","application/octet-stream");
    }else if(type=="text"){
        res.set_header("Content-Type","text/plain; charset=utf-8");
    }
    res.body.assign(buffer.begin(), buffer.end());
    res.code = 200;
    return res;
}

// NGINX に配信をオフロード（X-Accel-Redirect）するためのストリーミング用エンドポイント
// GET /req/media?type=video|audio|image|text&id=<dir_or_leaf_id>&filename=<name>&token=<jwt>
inline crow::response redirect_media(const crow::request& req){
    const char* type_c = req.url_params.get("type");
    const char* id_c   = req.url_params.get("id");
    const char* fn_c   = req.url_params.get("filename");
    if(!type_c || !id_c || !fn_c) return crow::response(400, "missing query");

    string type(type_c);
    string filename(fn_c);
    uint64_t idv;
    try{ idv = static_cast<uint64_t>(std::stoull(id_c)); }catch(...){ return crow::response(400, "bad id"); }

    // パス解決（サンドボックス: base_dir 配下）
    if(!(type=="image"||type=="video"||type=="audio"||type=="text"))
        return crow::response(400, "Unknown type");

    std::string base;
    {
        lock_guard<mutex> lock(imtex);
        Info* node=get_info_from_id(idv);
        if(!node || !valid_info_ptrs.contains(node)) return crow::response(404);
        if(type=="image" && !node->has_only_img) return crow::response(404);
        base = node->path;
    }

    std::filesystem::path fullpath = std::filesystem::path(base) / filename;
    std::error_code ec;
    auto canon_base = std::filesystem::weakly_canonical(std::filesystem::path(base_dir), ec);
    auto canon_fp   = std::filesystem::weakly_canonical(fullpath, ec);
    if(ec || canon_fp.empty() || canon_base.empty()) return crow::response(404);
    // base_dir 配下にあるかチェック
    auto canon_base_str = canon_base.generic_string();
    auto canon_fp_str   = canon_fp.generic_string();
    if(canon_fp_str.rfind(canon_base_str, 0) != 0) return crow::response(403);

    // 内部URI（NGINX 側で location /_protected_media/ { internal; alias <base_dir>/; } を設定）
    std::filesystem::path rel = std::filesystem::relative(canon_fp, canon_base, ec);
    if(ec) return crow::response(404);
    std::string internal_uri = std::string("/_protected_media/") + rel.generic_string();

    crow::response res;
    // Content-Type（任意: NGINX 側でも判定可能）
    if(type=="image"){
        if(filename.ends_with(".jpg")||filename.ends_with(".jpeg")) res.set_header("Content-Type","image/jpeg");
        else if(filename.ends_with(".png")) res.set_header("Content-Type","image/png");
        else if(filename.ends_with(".webp")) res.set_header("Content-Type","image/webp");
        else if(filename.ends_with(".gif")) res.set_header("Content-Type","image/gif");
        else res.set_header("Content-Type","application/octet-stream");
    }else if(type=="video"){
        if(filename.ends_with(".mp4")) res.set_header("Content-Type","video/mp4");
        else res.set_header("Content-Type","application/octet-stream");
    }else if(type=="audio"){
        if(filename.ends_with(".mp3")) res.set_header("Content-Type","audio/mpeg");
        else if(filename.ends_with(".flac")) res.set_header("Content-Type","audio/flac");
        else if(filename.ends_with(".aac")) res.set_header("Content-Type","audio/aac");
        else if(filename.ends_with(".wav")) res.set_header("Content-Type","audio/wav");
        else res.set_header("Content-Type","application/octet-stream");
    }else if(type=="text"){
        res.set_header("Content-Type","text/plain; charset=utf-8");
    }
    res.set_header("Accept-Ranges","bytes");
    res.set_header("X-Accel-Redirect", internal_uri);
    res.set_header("X-Content-Type-Options", "nosniff");
    res.code = 200;
    return res;
}

}// namespace comic