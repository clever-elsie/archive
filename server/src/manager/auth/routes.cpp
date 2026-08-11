#include <manager/auth/routes.hpp>

namespace AUTH{

void setup(App& app){
  CROW_ROUTE(app,"/req/auth/login")
    .methods(crow::HTTPMethod::POST)
      (AUTH::login_response);
  CROW_ROUTE(app,"/req/auth/logout")
    .methods(crow::HTTPMethod::POST)
      (AUTH::logout_response);
  CROW_ROUTE(app,"/req/auth/check")
    .methods(crow::HTTPMethod::GET, crow::HTTPMethod::POST)
      (AUTH::check_auth_response);
}

} // namespace AUTH
