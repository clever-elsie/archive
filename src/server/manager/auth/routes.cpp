#include <manager/auth/routes.hpp>

namespace AUTH{

void setup(crow::App<MIDDLEWARE::AuthMiddleware>& app){
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