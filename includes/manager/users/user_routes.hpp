#pragma once
#include "user_manager.hpp"
#include "user_api.hpp"
#include "crow.h"

using namespace std;

namespace USER_ROUTES {

template<typename Middleware>
inline void setup_user_routes(crow::App<Middleware>& app) {
  CROW_ROUTE(app,"/req/user/register")
    .methods(crow::HTTPMethod::POST)
      (USER_API::register_user);
  CROW_ROUTE(app,"/req/user/delete")
    .methods(crow::HTTPMethod::POST)
      (USER_API::delete_user);
  CROW_ROUTE(app,"/req/user/promote")
    .methods(crow::HTTPMethod::POST)
      (USER_API::promote_user);
  CROW_ROUTE(app,"/req/user/demote")
    .methods(crow::HTTPMethod::POST)
      (USER_API::demote_user);
  CROW_ROUTE(app,"/req/user/list")
    .methods(crow::HTTPMethod::GET)
      (USER_API::get_user_list);
  CROW_ROUTE(app,"/req/user/check_first")
    .methods(crow::HTTPMethod::GET)
      (USER_API::check_first_user);
  CROW_ROUTE(app,"/req/user/permissions")
    .methods(crow::HTTPMethod::POST)
      (USER_API::get_user_permissions);
}

} // namespace USER_ROUTES 