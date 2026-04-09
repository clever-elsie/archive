#pragma once
#include <string>

#include <crow.h>
#include <crow/json.h>

#include <manager/auth/auth.hpp>
#include <manager/auth/middleware.hpp>
#include <manager/users/manager.hpp>

namespace MIDDLEWARE {
using namespace std;

// 認可（権限）ミドルウェア。
// 認証(AuthMiddleware)の後段に置き、URL(method含む)→必要権限を一元判定する。
struct AuthorizationMiddleware {
  struct context {};

  static inline crow::response json_error(int code, const char* msg, const char* err_code) {
    crow::json::wvalue w;
    w["error"] = msg;
    w["code"] = err_code;
    return crow::response(code, w);
  }

  static inline bool token_valid(const string& token) {
    return !token.empty() && AUTH::validate_token_wrapper(token);
  }

  static inline string username_from_req(const crow::request& req) {
    string token = MIDDLEWARE::extract_token(req);
    if (!token_valid(token)) return "";
    return AUTH::get_username_from_token(token);
  }

  static inline bool require_admin(const crow::request& req, crow::response& res) {
    const string username = username_from_req(req);
    if (username.empty()) {
      res = json_error(401, "認証が必要です", "AUTH_REQUIRED");
      res.end();
      return false;
    }
    if (!USER_MANAGER::user_manager.is_admin(username)) {
      res = json_error(403, "管理者権限が必要です", "ADMIN_REQUIRED");
      res.end();
      return false;
    }
    return true;
  }

  void before_handle(crow::request& req, crow::response& res, context& ctx) {
    // OPTIONSは認可不要（AuthMiddleware側と同様にスキップ）
    if (req.method == crow::HTTPMethod::OPTIONS) return;

    const string path = req.url;

    // ===== viewer (admin only) =====
    if (path == "/req/img/reload" && req.method == crow::HTTPMethod::POST) {
      (void)require_admin(req, res);
      return;
    }
    if (path == "/req/img/info_renew" && req.method == crow::HTTPMethod::PATCH) {
      (void)require_admin(req, res);
      return;
    }

    // ===== user management =====
    // 既存実装が関数内で権限チェックしていた/すべきものを、URL単位で集約。
    if (path == "/req/user/list" && req.method == crow::HTTPMethod::GET) {
      (void)require_admin(req, res);
      return;
    }
    if ((path == "/req/user/delete" || path == "/req/user/promote" || path == "/req/user/demote") &&
        req.method == crow::HTTPMethod::POST) {
      (void)require_admin(req, res);
      return;
    }

    // register は「初回ユーザー(admin)登録」だけは無認証で許可するが、
    // 2人目以降の登録は認証+権限が必要。
    if (path == "/req/user/register" && req.method == crow::HTTPMethod::POST) {
      const bool is_first_user = USER_MANAGER::user_manager.is_first_user();
      if (is_first_user) return;

      const string username = username_from_req(req);
      if (username.empty()) {
        res = json_error(401, "認証が必要です", "AUTH_REQUIRED");
        res.end();
        return;
      }

      // 登録対象の role に応じた権限を要求（既存UserManagerのルールに従う）
      const crow::json::rvalue data = crow::json::load(req.body);
      const string role = data.has("role") ? data["role"].s() : string();
      if (role == "admin") {
        if (!USER_MANAGER::user_manager.can_register_admin(username)) {
          res = json_error(403, "管理者登録の権限がありません", "FORBIDDEN");
          res.end();
          return;
        }
      } else if (role == "user") {
        if (!USER_MANAGER::user_manager.can_register_user(username)) {
          res = json_error(403, "ユーザー登録の権限がありません", "FORBIDDEN");
          res.end();
          return;
        }
      } else {
        // roleが無い/不正な場合は、ハンドラ側で400を返す想定だが、
        // ここでは認可だけ扱うので通す。
      }
      return;
    }
  }

  void after_handle(crow::request& req, crow::response& res, context& ctx) {}
};

} // namespace MIDDLEWARE

