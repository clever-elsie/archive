#include <fstream>

#include <crow/multipart.h>

#include <manager/auth/middleware.hpp>
#include <manager/auth/auth.hpp>
#include <manager/users/manager.hpp>
#include <app/retrieve.hpp>
#include <inline_helper.hpp>
#include <app/viewer/inline_helper.hpp>
#include <app/viewer/manager.hpp>
#include <app/viewer/diraccess.hpp>

namespace VIEWER{
using namespace std;

crow::json::wvalue get_imgs(const crow::request&req){
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	auto data = crow::json::load(req.body);
	uint64_t idv = static_cast<uint64_t>(data["id"].i());
	Info* node = mgr.get_info_from_id(idv);
	if(!mgr.is_valid(node) || !node->has_only_img||!node->refresh(0)) return crow::json::wvalue();
	const std::string bpath(node->path);
	vector<string>& imgs=node->imgs;
	crow::json::wvalue::list img_list;
	crow::json::wvalue ret;
	for(auto const & img:imgs){
		crow::json::wvalue next;
		next["img"]=filesystem::relative(filesystem::path(bpath)/img,mgr.base_dir);
		img_list.push_back(move(next));
	}
	ret["img"]=move(img_list);
	const string info=node->path/filesystem::path(".info");
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

crow::json::wvalue get_dir_list(const crow::request&req){
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	namespace F = std::filesystem;
	auto data = crow::json::load(req.body);
	const uint64_t idv=static_cast<uint64_t>(data["id"].i());
	std::string order_key = data.has("order_key") ? data["order_key"].s() : std::string("name");
	std::string order = data.has("order") ? data["order"].s() : std::string("ascendant");
	Info* tar=mgr.get_info_from_id(idv);
	if(!mgr.is_valid(tar)||!tar->refresh(0)) return crow::json::wvalue();
	crow::json::wvalue ret;
	ret["cur"]=idv;
	ret["par"]=tar->par->id();

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
		next["id"]=d->id();
		dir.emplace_back(move(next));
	}
	ret["imgs"]=crow::json::wvalue(move(img));
	ret["dirs"]=crow::json::wvalue(move(dir));
	return ret;
}

} // namespace VIEWER