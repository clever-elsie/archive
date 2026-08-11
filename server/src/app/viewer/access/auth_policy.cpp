#include <app/viewer/manager.hpp>

#include <manager/auth/auth.hpp>
#include <manager/auth/middleware.hpp>
#include <manager/users/manager.hpp>

namespace VIEWER {

bool Manager::is_admin_request(const crow::request& req) const {
  const auto token = MIDDLEWARE::extract_token(req);
  if (token.empty() || !AUTH::validate_token_wrapper(token)) return false;
  const auto username = AUTH::get_username_from_token(token);
  return !username.empty() && USER_MANAGER::get_user_manager().is_admin(username);
}

} // namespace VIEWER
