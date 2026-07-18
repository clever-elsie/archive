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
#include <app/viewer/access_control.hpp>

namespace VIEWER{
using namespace std;

crow::json::wvalue get_imgs(const crow::request&req){
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	const char* id_c = req.url_params.get("id");
	if(!id_c) return crow::json::wvalue();
	uint64_t idv;
	try {
		idv = static_cast<uint64_t>(std::stoull(id_c));
	} catch(...) {
		return crow::json::wvalue();
	}
	Info* node = mgr.get_info_from_id(idv);
	if(!mgr.is_valid(node)
		|| !node->is_trackable()
		||!VIEWER::can_view_node(req, node->parent())
	)
		return crow::json::wvalue();
	crow::json::wvalue ret;
	ret["id"]=idv;
	ret["dir_type"]=Info::directory_type_to_string(node->directory_type());

	// 画像リスト
	crow::json::wvalue::list img_list;
	for(auto const & img:node->media_relative_paths<Info::MediaType::image>()){
		crow::json::wvalue next;
		next["img"]=img;
		img_list.push_back(std::move(next));
	}
	ret["img"]=std::move(img_list);

	// 各カテゴリのリスト
	auto add_media_list = [&](const char* key, Info::MediaType type) {
		crow::json::wvalue::list list;
		for(auto const & path : node->media_relative_paths(type)) {
			crow::json::wvalue next;
			next["path"] = path;
			next["filename"] = std::filesystem::path(path).filename().string();
			next["id"] = idv;
			list.push_back(std::move(next));
		}
		ret[key] = std::move(list);
	};
	add_media_list("videos", Info::MediaType::video);
	add_media_list("audios", Info::MediaType::audio);
	add_media_list("texts", Info::MediaType::text);
	add_media_list("pdfs", Info::MediaType::doc);

	// タグ
	crow::json::wvalue::list ts;
	for(const auto&x:node->normalized_tags())
		ts.push_back(x);
	ret["tags"]=crow::json::wvalue(ts);

	crow::json::wvalue::list parent_list;
	auto type = node->directory_type();
	if (type == DirectoryType::only_movies || type == DirectoryType::only_musics || type == DirectoryType::only_text || type == DirectoryType::only_pdfs) {
		if (type == DirectoryType::only_movies) {
			for(auto const & vid : node->media_relative_paths(Info::MediaType::video)) {
				crow::json::wvalue next;
				next["id"] = idv;
				next["img"] = "";
				next["click_action"] = "play_media";
				next["media_type"] = "video";
				next["media_path"] = vid;
				next["dirname"] = std::filesystem::path(vid).filename().string();
				next["dir_type"] = "file";
				parent_list.push_back(std::move(next));
			}
		} else if (type == DirectoryType::only_musics) {
			for(auto const & aud : node->media_relative_paths(Info::MediaType::audio)) {
				crow::json::wvalue next;
				next["id"] = idv;
				next["img"] = "";
				next["click_action"] = "play_media";
				next["media_type"] = "audio";
				next["media_path"] = aud;
				next["dirname"] = std::filesystem::path(aud).filename().string();
				next["dir_type"] = "file";
				parent_list.push_back(std::move(next));
			}
		} else if (type == DirectoryType::only_text) {
			for(auto const & txt : node->media_relative_paths(Info::MediaType::text)) {
				crow::json::wvalue next;
				next["id"] = idv;
				next["img"] = "";
				next["click_action"] = "play_media";
				next["media_type"] = "text";
				next["media_path"] = txt;
				next["dirname"] = std::filesystem::path(txt).filename().string();
				next["dir_type"] = "file";
				parent_list.push_back(std::move(next));
			}
		} else if (type == DirectoryType::only_pdfs) {
			for(auto const & doc : node->media_relative_paths(Info::MediaType::doc)) {
				crow::json::wvalue next;
				next["id"] = idv;
				next["img"] = "";
				next["click_action"] = "play_media";
				next["media_type"] = "doc";
				next["media_path"] = doc;
				next["dirname"] = std::filesystem::path(doc).filename().string();
				next["dir_type"] = "file";
				parent_list.push_back(std::move(next));
			}
		}
	} else {
		for(const auto& d : node->parent()->get_dirs()) {
			if (d->is_trackable()) {
				pb_next(parent_list, d.get());
			}
		}
	}
	ret["parent"] = std::move(parent_list);
	return crow::json::wvalue(ret);
}

crow::json::wvalue get_dir_list(const crow::request&req){
	manager& mgr = manager::get_instance();
	lock_guard<mutex> lock(mgr.imtex);
	namespace F = std::filesystem;

	const char* id_c = req.url_params.get("id");
	if(!id_c) return crow::json::wvalue();
	uint64_t idv;
	try {
		idv = static_cast<uint64_t>(std::stoull(id_c));
	} catch(...) {
		return crow::json::wvalue();
	}
	Info* tar=mgr.get_info_from_id(idv);
	if(!mgr.is_valid(tar)) return crow::json::wvalue();

	if(!VIEWER::can_view_node(req, tar))
		return crow::json::wvalue();

	const char* order_key_c = req.url_params.get("order_key");
	const char* order_c = req.url_params.get("order");
	const Info::SortingOrder order_type = (order_key_c && std::string(order_key_c) == "last_write_time") ? Info::SortingOrder::last_write_time : Info::SortingOrder::name;
	const bool descendant = order_c && std::string(order_c) == "descendant";

	crow::json::wvalue ret;
	ret["cur"]=idv;
	ret["par"]=tar->parent_id();

	const auto[imgvec,dirvec]=tar->imgdirs_or_elsedirs(order_type,descendant);
	crow::json::wvalue::list dir,img;
	for(const auto&an_img:imgvec){
		if(VIEWER::can_view_node(req, an_img)) {
			crow::json::wvalue next;
			next["img"] = an_img->current_thumbnail_relative_path();
			next["id"] = an_img->id();
			next["click_action"] = "gallery";
			next["dir_type"] = Info::directory_type_to_string(an_img->directory_type());
			next["dirname"] = an_img->dirname();
			img.push_back(std::move(next));
		}
	}
	const auto& imgs=tar->media_relative_paths<Info::MediaType::image>(); // 画像だけは順序指定を無視して名前昇順
	for(const auto&an_img:imgs)
		pb_next(img, an_img, tar->id());

	for(const auto&d:dirvec){
		if(!VIEWER::can_view_node(req, d)) continue;
		crow::json::wvalue next;
		next["path"]=std::move(d->relative_path());
		next["dirname"]=std::move(d->dirname());
		next["id"]=d->id();
		dir.emplace_back(std::move(next));
	}
	ret["imgs"]=crow::json::wvalue(std::move(img));
	ret["dirs"]=crow::json::wvalue(std::move(dir));

	// JSONに変換
	crow::json::wvalue::list media_list;
	for(const auto&type:{Info::MediaType::video, Info::MediaType::audio, Info::MediaType::text, Info::MediaType::doc})
		for(const auto&path:tar->media_relative_paths(type,order_type,descendant)){
			crow::json::wvalue next;
			next["path"] = path;
			next["filename"] = std::filesystem::path(path).filename().string();
			next["type"] = std::string(Info::mt_string(type));
			next["id"] = tar->id();
			media_list.emplace_back(std::move(next));
		}
	ret["media"] = crow::json::wvalue(std::move(media_list));
	return ret;
}

} // namespace VIEWER