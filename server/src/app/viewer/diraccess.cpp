#include <fstream>
#include <ranges>

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
	if(!mgr.is_valid(node) || !node->has_only_img()||!node->refresh(0)) return crow::json::wvalue();
	const std::string bpath(node->path);
	const vector<string>& imgs=node->imgs();
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
	std::vector<size_t> img_range;
	for(const auto&d:tar->dirs)
		(d->has_only_img()?imgvec:dirvec).push_back(d.get());
	if(!tar->has_only_img())
		for(auto id:std::views::iota(0U,tar->imgs().size()))
			img_range.push_back(id);
	// その他メディア（video, audio, text, doc）を1つの配列にまとめる
	// ソート用の構造体
	struct MediaItem {
		std::string name;
		std::string type;
		filesystem::path full_path;
		filesystem::file_time_type last_write_time;
	};
	std::vector<MediaItem> media_items;
	
	// メディアアイテムを収集
	auto add_media_items = [&](const std::vector<std::string>& files, const std::string& type) {
		for(const auto& name : files) {
			filesystem::path file_path = tar->path / name;
			try {
				if(filesystem::exists(file_path)) {
					MediaItem item;
					item.name = name;
					item.type = type;
					item.full_path = file_path;
					item.last_write_time = filesystem::last_write_time(file_path);
					media_items.push_back(item);
				}
			} catch(...) {
				// エラー時はスキップ
			}
		}
	};
	
	add_media_items(tar->media_vector(Info::MediaType::video), "video");
	add_media_items(tar->media_vector(Info::MediaType::audio), "audio");
	add_media_items(tar->media_vector(Info::MediaType::text), "text");
	add_media_items(tar->media_vector(Info::MediaType::doc), "doc");
	
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
		if(order_key!="last_write_time")
			std::reverse(img_range.begin(), img_range.end());
	}

	// ソート処理（画像とディレクトリと同じロジック）
	if(order_key=="last_write_time"){
		auto cmp=[](const MediaItem& a, const MediaItem& b){
			if(a.last_write_time==b.last_write_time)
				return a.full_path < b.full_path;
			return a.last_write_time < b.last_write_time;
		};
		std::sort(media_items.begin(), media_items.end(), cmp);
	} else {
		// order_key=="name"の場合、名前でソート（元々名前昇順だが明示的にソート）
		auto cmp=[](const MediaItem& a, const MediaItem& b){
			return a.full_path < b.full_path;
		};
		std::sort(media_items.begin(), media_items.end(), cmp);
	}
	if(order=="descendant"){
		std::reverse(media_items.begin(), media_items.end());
	}
	
	crow::json::wvalue::list dir,img;
	for(const auto&d:imgvec) pb_next(img,*d);
	for(const auto&i:img_range) pb_next(img,*tar,i);
	for(const auto&d:dirvec){
		crow::json::wvalue next;
		next["path"]=(d->path);
		next["dirname"]=(filesystem::path(d->path).filename());
		next["id"]=d->id();
		dir.emplace_back(move(next));
	}
	ret["imgs"]=crow::json::wvalue(move(img));
	ret["dirs"]=crow::json::wvalue(move(dir));

	// JSONに変換
	crow::json::wvalue::list media_list;
	for(const auto& item : media_items) {
		crow::json::wvalue next;
		next["path"] = filesystem::relative(item.full_path, mgr.base_dir);
		next["filename"] = item.name;
		next["type"] = item.type;
		next["id"] = tar->id();
		media_list.emplace_back(std::move(next));
	}
	ret["media"] = crow::json::wvalue(std::move(media_list));
	return ret;
}

} // namespace VIEWER