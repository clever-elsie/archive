#pragma once
#include "memo.hpp"
#include "../../headers.hpp"
#include <crow.h>

namespace MEMO_ROUTES{

template<typename Middleware>
inline void setup_memo_routes(crow::App<Middleware>& app){
	namespace fs = std::filesystem;
	MEMO::buf_path = fs::canonical(fs::current_path() / "memo" / "buf").string() + "/";

  CROW_ROUTE(app,"/req/memo/all")
    .methods(crow::HTTPMethod::GET)
      (MEMO::memo_fetch_all);
  CROW_ROUTE(app,"/req/memo/new_id")
    .methods(crow::HTTPMethod::GET)
      (MEMO::memo_issue_new_id);
  CROW_ROUTE(app,"/req/memo/renew")
    .methods(crow::HTTPMethod::POST)
      (MEMO::memo_renew);
  CROW_ROUTE(app,"/req/memo/now")
    .methods(crow::HTTPMethod::GET)
      (MEMO::memo_now);
  CROW_ROUTE(app,"/req/memo/remove")
    .methods(crow::HTTPMethod::POST)
      (MEMO::memo_rm);
}

} // namespace MEMO_ROUTES