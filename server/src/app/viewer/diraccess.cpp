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
		|| !node->has_only_img()
		||!VIEWER::can_view_node(req, node->parent())
	)
		return crow::json::wvalue();
	crow::json::wvalue ret;
	ret["id"]=idv;

	// 画像リスト
	crow::json::wvalue::list img_list;
	for(auto const & img:node->media_relative_paths<Info::MediaType::image>()){
		crow::json::wvalue next;
		next["img"]=img;
		img_list.push_back(std::move(next));
	}
	ret["img"]=std::move(img_list);

	// タグ
	crow::json::wvalue::list ts;
	for(const auto&x:node->normalized_tags())
		ts.push_back(x);
	ret["tags"]=crow::json::wvalue(ts);

	// 親ディレクトリの全画像ディレクトリサムネイル
	crow::json::wvalue::list parent_list;
	const auto& thumbnail_paths = node->parent_all_thumbnail_relative_paths();
	const auto& parent_imgdirs = node->parent()->imgdirs_or_elsedirs().first;
	for(const auto&d:parent_imgdirs)
		pb_next(parent_list, d->current_thumbnail_relative_path(), d->id());
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
		if(VIEWER::can_view_node(req, an_img))
			pb_next(img, an_img->current_thumbnail_relative_path(), an_img->id());
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