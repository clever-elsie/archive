#pragma once
#include "memo.hpp"
#include <crow.h>

namespace MEMO_ROUTES{

template<typename Middleware>
inline void setup_memo_routes(crow::App<Middleware>& app){
	namespace fs = std::filesystem;
	MEMO::memo_base_path = fs::canonical(fs::current_path() / "memo").string() + "/";

  CROW_ROUTE(app,"/req/memo/all")
    .methods(crow::HTTPMethod::GET)
      (MEMO::memo_fetch_all);
  CROW_ROUTE(app,"/req/memo/search")
    .methods(crow::HTTPMethod::POST)
      (MEMO::memo_search);
  CROW_ROUTE(app,"/req/memo/create")
    .methods(crow::HTTPMethod::POST)
      (MEMO::memo_create_new);
  CROW_ROUTE(app,"/req/memo/create_with_title")
    .methods(crow::HTTPMethod::POST)
      (MEMO::memo_create_with_title);
  CROW_ROUTE(app,"/req/memo/check_title")
    .methods(crow::HTTPMethod::POST)
      (MEMO::memo_check_title);
  CROW_ROUTE(app,"/req/memo/renew")
    .methods(crow::HTTPMethod::POST)
      (MEMO::memo_renew);
  CROW_ROUTE(app,"/req/memo/now")
    .methods(crow::HTTPMethod::POST)
      (MEMO::memo_now);
  CROW_ROUTE(app,"/req/memo/remove")
    .methods(crow::HTTPMethod::POST)
      (MEMO::memo_rm);
  CROW_ROUTE(app,"/req/memo/rename")
    .methods(crow::HTTPMethod::POST)
      (MEMO::memo_rename);
  CROW_ROUTE(app,"/req/memo/update_tags")
    .methods(crow::HTTPMethod::POST)
      (MEMO::memo_update_tags);
  CROW_ROUTE(app,"/req/memo/formats")
    .methods(crow::HTTPMethod::GET)
      (MEMO::memo_get_formats);
  
  // 共用メモ関連のルート
  CROW_ROUTE(app,"/req/shared-memo/all")
    .methods(crow::HTTPMethod::GET)
      (MEMO::shared_memo_fetch_all);
  CROW_ROUTE(app,"/req/shared-memo/create")
    .methods(crow::HTTPMethod::POST)
      (MEMO::shared_memo_create);
  CROW_ROUTE(app,"/req/shared-memo/update")
    .methods(crow::HTTPMethod::POST)
      (MEMO::shared_memo_update);
  CROW_ROUTE(app,"/req/shared-memo/delete")
    .methods(crow::HTTPMethod::POST)
      (MEMO::shared_memo_delete);
  CROW_ROUTE(app,"/req/shared-memo/get")
    .methods(crow::HTTPMethod::POST)
      (MEMO::shared_memo_get);
}

} // namespace MEMO_ROUTES