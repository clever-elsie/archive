#pragma once
#include "auth.hpp"
#include "../config.hpp"
#include <crow.h>

namespace AUTH{

template<typename Middleware>
inline void setup_auth_routes(crow::App<Middleware>& app){
  CROW_ROUTE(app,"/req/auth/login")
    .methods(crow::HTTPMethod::POST)
      (AUTH::login_response);
  CROW_ROUTE(app,"/req/auth/logout")
    .methods(crow::HTTPMethod::POST)
      (AUTH::logout_response);
  CROW_ROUTE(app,"/req/auth/check")
    .methods(crow::HTTPMethod::POST)
      (AUTH::check_auth_response);
}

} // namespace AUTH