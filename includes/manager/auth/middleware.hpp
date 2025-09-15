#pragma once
#include "manager/auth/auth.hpp"
#include "manager/config.hpp"
#include <string>
#include <crow.h>
#include <vector>

namespace MIDDLEWARE {
using namespace std;

bool requires_auth(const string& path); // 認証が必要かどうかを判定
string extract_token(const crow::request& req); // リクエストからJWTトークンを抽出
// CSRF検証が必要かどうかを判定
bool requires_csrf_validation(const string& path, const crow::HTTPMethod& method);
bool is_allowed_origin(const string& origin); // CORS検証
// 許可されたオリジンを取得（CORSヘッダー用）
string get_allowed_origin(const string& request_origin);

struct AuthMiddleware { // 認証ミドルウェアクラス
  struct context {};
  void before_handle(crow::request& req, crow::response& res, context& ctx);
  void after_handle(crow::request& req, crow::response& res, context& ctx);
};

} // namespace MIDDLEWARE 