#include <app/viewer/routes.hpp>

namespace VIEWER_ROUTES{
void setup_viewer_routes(crow::App<MIDDLEWARE::AuthMiddleware>& app, std::string&& viewer_dir){
  namespace fs = std::filesystem;
  VIEWER::base_dir = std::move(viewer_dir);
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
  CROW_ROUTE(app,"/req/img/file")
    .methods(crow::HTTPMethod::POST)
      (VIEWER::get_file_binary);

  // X-Accel-Redirect を利用した配信用（GET）。
  CROW_ROUTE(app,"/req/media")
    .methods(crow::HTTPMethod::GET)
      (VIEWER::redirect_media);
}
} // namespace VIEWER_ROUTES