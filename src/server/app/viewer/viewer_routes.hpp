#pragma once
#include "viewer.hpp"
#include "../../headers.hpp"
#include <crow.h>

namespace VIEWER_ROUTES{

template<typename Middleware>
inline void setup_viewer_routes(crow::App<Middleware>& app){
  namespace fs = std::filesystem;
	VIEWER::base_dir = fs::canonical(fs::current_path() / "data").string();
	VIEWER::rel_base = fs::canonical(fs::current_path()).string()+'/';
	VIEWER::base_time = fs::last_write_time(VIEWER::base_dir);
	VIEWER::load_leaf_dir(VIEWER::base_dir);
  CROW_ROUTE(app,"/req/img/rand")
    .methods(crow::HTTPMethod::GET)
      (VIEWER::get_rand_imgs);
  CROW_ROUTE(app,"/req/img")
    .methods(crow::HTTPMethod::POST)
      (VIEWER::get_imgs);
  CROW_ROUTE(app,"/req/img/page_list")
    .methods(crow::HTTPMethod::GET)
      (VIEWER::get_page_list);
  CROW_ROUTE(app,"/req/img/page")
    .methods(crow::HTTPMethod::POST)
      (VIEWER::get_page);
  CROW_ROUTE(app,"/req/img/retrieve")
    .methods(crow::HTTPMethod::POST)
      (VIEWER::retrieve_query);
  CROW_ROUTE(app,"/req/img/dir_access")
    .methods(crow::HTTPMethod::POST)
      (VIEWER::get_dir_list);
  // ファイル構造キャッシュリロード
  CROW_ROUTE(app,"/req/img/reload")
    .methods(crow::HTTPMethod::GET)
      (VIEWER::reload_leaf);
  // タグ，作成者更新
  CROW_ROUTE(app,"/req/img/info_renew")
    .methods(crow::HTTPMethod::POST)
      (VIEWER::info_renew);
}

} // namespace VIEWER_ROUTES