#pragma once
#include <cstdint>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>

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

  using Handler = bool (*)(const crow::request&, crow::response&);

  struct Entry {
    std::string_view path;
    Handler handler;
  };

  struct RegexEntry {
    uint8_t mid;
    std::regex pattern;
    Handler handler;
  };

  static inline crow::response json_error(int code, const char* msg, const char* err_code) {
    crow::json::wvalue w;
    w["error"] = msg;
    w["code"] = err_code;
    return crow::response(code, w);
  }

  static inline uint8_t method_id(crow::HTTPMethod m) {
    // crow::HTTPMethod は enum だが値はABIに依存しうるため、ここで安定な小整数へ写像する
    switch (m) {
      case crow::HTTPMethod::GET: return 1;
      case crow::HTTPMethod::POST: return 2;
      case crow::HTTPMethod::PUT: return 3;
      case crow::HTTPMethod::PATCH: return 4;
      case crow::HTTPMethod::DELETE: return 5;
      case crow::HTTPMethod::OPTIONS: return 6;
      default: return 0;
    }
  }

  static inline uint64_t key_of(uint8_t mid, std::string_view path) {
    // 第一段階: (method, hash(path)) で候補集合を引く（Rabin–Karp的）
    const uint64_t h = static_cast<uint64_t>(std::hash<std::string_view>{}(path));
    return (static_cast<uint64_t>(mid) << 56) ^ h;
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

  static inline bool handle_user_register(const crow::request& req, crow::response& res) {
    // register は「初回ユーザー(admin)登録」だけは無認証で許可するが、
    // 2人目以降の登録は認証+権限が必要。
    const bool is_first_user = USER_MANAGER::user_manager.is_first_user();
    if (is_first_user) return true;

    const string username = username_from_req(req);
    if (username.empty()) {
      res = json_error(401, "認証が必要です", "AUTH_REQUIRED");
      res.end();
      return false;
    }

    const crow::json::rvalue data = crow::json::load(req.body);
    if (!data) {
      res = json_error(400, "リクエストの形式が不正です", "BAD_REQUEST");
      res.end();
      return false;
    }

    const string role = data.has("role") ? data["role"].s() : string();
    if (role == "admin") {
      if (!USER_MANAGER::user_manager.can_register_admin(username)) {
        res = json_error(403, "管理者登録の権限がありません", "FORBIDDEN");
        res.end();
        return false;
      }
    } else if (role == "user") {
      if (!USER_MANAGER::user_manager.can_register_user(username)) {
        res = json_error(403, "ユーザー登録の権限がありません", "FORBIDDEN");
        res.end();
        return false;
      }
    }
    // roleが無い/不正な場合はハンドラ側で400を返す想定なので、ここでは認可として通す
    return true;
  }

  static inline bool h_require_admin(const crow::request& req, crow::response& res) {
    return require_admin(req, res);
  }

  static inline const std::unordered_multimap<uint64_t, Entry>& rules() {
    // ルールは「(method, hash(path)) → 候補集合 → path厳密一致 → handler」の二段階で判定する。
    // /req/user/register は初回例外があるため、テーブル外で処理する。
    static const std::unordered_multimap<uint64_t, Entry> tbl = []() {
      std::unordered_multimap<uint64_t, Entry> t;
      auto add = [&](crow::HTTPMethod m, std::string_view p, Handler h) {
        t.emplace(key_of(method_id(m), p), Entry{p, h});
      };

      // viewer (admin only)
      add(crow::HTTPMethod::POST,  "/req/img/reload",    &h_require_admin);
      add(crow::HTTPMethod::PATCH, "/req/img/info_renew",&h_require_admin);
      add(crow::HTTPMethod::POST, "/req/img/rand/<int>", &h_require_admin);
      add(crow::HTTPMethod::POST, "/req/img/page_data", &h_require_admin);

      // user management (admin only)
      add(crow::HTTPMethod::GET,  "/req/user/list",   &h_require_admin);
      add(crow::HTTPMethod::POST, "/req/user/delete", &h_require_admin);
      add(crow::HTTPMethod::POST, "/req/user/promote",&h_require_admin);
      add(crow::HTTPMethod::POST, "/req/user/demote", &h_require_admin);

      return t;
    }();
    return tbl;
  }

  static inline const std::vector<RegexEntry>& regex_rules() {
    // パラメータ付きルートなど、完全一致に落とせないものをregexで扱う。
    // 数は少数の想定なので線形走査で十分。
    static const std::vector<RegexEntry> rs = []() {
      std::vector<RegexEntry> v;
      auto add = [&](crow::HTTPMethod m, const char* re, Handler h) {
        v.push_back(RegexEntry{method_id(m), std::regex(re), h});
      };

      // viewer (admin only)
      // /req/img/rand/<int> → /req/img/rand/123 のような実URLを許可/拒否したい
      add(crow::HTTPMethod::POST, R"(^/req/img/rand/\d+$)", &h_require_admin);

      return v;
    }();
    return rs;
  }

  void before_handle(crow::request& req, crow::response& res, context& ctx) {
    // OPTIONSは認可不要（AuthMiddleware側と同様にスキップ）
    if (req.method == crow::HTTPMethod::OPTIONS) return;

    const std::string_view path(req.url);
    const uint8_t mid = method_id(req.method);

    // 例外: 初回登録の特例があるため専用処理
    if (path == "/req/user/register" && req.method == crow::HTTPMethod::POST) {
      (void)handle_user_register(req, res);
      return;
    }

    const auto& tbl = rules();
    const uint64_t key = key_of(mid, path);
    auto range = tbl.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
      const Entry& e = it->second;
      if (e.path != path) continue; // hash衝突は厳密一致で排除
      if (!e.handler(req, res)) return; // handler側でres.end済み
      return;
    }

    // 完全一致に無ければ regex ルールを評価
    for (const auto& r : regex_rules()) {
      if (r.mid != mid) continue;
      if (!std::regex_match(std::string(path), r.pattern)) continue;
      (void)r.handler(req, res);
      return;
    }
  }

  void after_handle(crow::request& req, crow::response& res, context& ctx) {}
};

} // namespace MIDDLEWARE

