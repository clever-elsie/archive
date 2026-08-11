#pragma once

#include <string>
#include <string_view>

#include <crow.h>
#include <crow/json.h>

#include <manager/auth/auth.hpp>
#include <manager/users/manager.hpp>

namespace MIDDLEWARE {

struct AuthorizationMiddleware {
  struct context {};

  static crow::response json_error(
      int status,
      const char* message,
      const char* code) {
    crow::json::wvalue body;
    body["success"] = false;
    body["code"] = code;
    body["message"] = message;
    body["error"] = message;
    body["data"] = nullptr;
    return crow::response(status, std::move(body));
  }

  static bool require_admin(
      const crow::request& req,
      crow::response& res) {
    const auto principal = AUTH::principal_from_request(req);
    if (!principal) {
      res = json_error(401, "認証が必要です", "AUTH_REQUIRED");
      res.end();
      return false;
    }
    if (!principal->is_admin()) {
      res = json_error(403, "管理者権限が必要です", "ADMIN_REQUIRED");
      res.end();
      return false;
    }
    return true;
  }

  static bool admin_route(
      std::string_view path,
      crow::HTTPMethod method) {
    if (path == "/req/user/list" && method == crow::HTTPMethod::GET)
      return true;
    if ((path == "/req/user/delete" ||
         path == "/req/user/promote" ||
         path == "/req/user/demote" ||
         path == "/req/viewer/reload") &&
        method == crow::HTTPMethod::POST)
      return true;
    if (path == "/req/viewer/random" && method == crow::HTTPMethod::GET)
      return true;
    if (path.starts_with("/req/viewer/entries/") &&
        path.ends_with("/metadata") &&
        method == crow::HTTPMethod::PATCH)
      return true;
    return false;
  }

  void before_handle(
      crow::request& req,
      crow::response& res,
      context&) {
    if (req.method == crow::HTTPMethod::OPTIONS)
      return;

    const std::string_view path(req.url);
    if (path == "/req/user/register" &&
        req.method == crow::HTTPMethod::POST) {
      if (USER_MANAGER::get_user_manager().is_first_user())
        return;
      (void)require_admin(req, res);
      return;
    }
    if (admin_route(path, req.method))
      (void)require_admin(req, res);
  }

  void after_handle(
      crow::request&,
      crow::response&,
      context&) {}
};

} // namespace MIDDLEWARE
