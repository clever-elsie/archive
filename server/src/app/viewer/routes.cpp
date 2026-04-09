#include <thread>
#include <app/viewer/routes.hpp>
#include <app/viewer/random.hpp>
#include <app/viewer/page.hpp>
#include <app/viewer/loader.hpp>
#include <app/viewer/file_server.hpp>
#include <app/viewer/retrieve.hpp>
#include <app/viewer/tag.hpp>
#include <app/viewer/diraccess.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER{
void setup(App& app, std::string&& viewer_dir){
  namespace fs = std::filesystem;
  manager&mgr = manager::get_instance();
  mgr.base_dir = std::move(viewer_dir);
  mgr.start_initial_load(mgr.base_dir);

  CROW_ROUTE(app,"/req/img/rand/<int>")
    .methods(crow::HTTPMethod::POST)
      (VIEWER::get_rand_imgs);
  CROW_ROUTE(app,"/req/img")
    .methods(crow::HTTPMethod::POST)
      (VIEWER::get_imgs);
  CROW_ROUTE(app,"/req/img/page_data")
    .methods(crow::HTTPMethod::POST)
      (VIEWER::get_page_data);
  CROW_ROUTE(app,"/req/img/retrieve")
    .methods(crow::HTTPMethod::POST)
      (VIEWER::retrieve_query);
  CROW_ROUTE(app,"/req/img/dir_access")
    .methods(crow::HTTPMethod::POST)
      (VIEWER::get_dir_list);
  // ファイル構造キャッシュリロード
  CROW_ROUTE(app,"/req/img/reload")
    .methods(crow::HTTPMethod::POST)
      (VIEWER::reload_leaf);
  // タグ更新
  CROW_ROUTE(app,"/req/img/info_renew")
    .methods(crow::HTTPMethod::PATCH)
      (VIEWER::info_renew);

  // X-Accel-Redirect を利用した配信用（GET）。
  CROW_ROUTE(app,"/req/media")
    .methods(crow::HTTPMethod::GET)
      (VIEWER::redirect_media);
}
} // namespace VIEWER