#pragma once
#include "manager/config.hpp"
#include "manager/users/user_manager.hpp"
#include "crow/http_response.h"
#include "crow/http_request.h"
#include "jwt.hpp"

namespace AUTH {
using namespace std;

// JWTトークン生成
inline string generate_token(const string& username) {
  return JWT::generate_token(username, CONFIG::params.JWT_SECRET_KEY);
}

// JWTトークン検証
inline bool validate_token(const string& token) {
  if (token.empty()) return false;
  // トークンの署名を検証
  if (!JWT::verify_token(token, CONFIG::params.JWT_SECRET_KEY))
    return false;
  // トークンの有効期限をチェック
  if (JWT::is_token_expired(token)) return false;
  return true;
}

// トークンからユーザー名を取得
inline string get_username_from_token(const string& token) {
  return JWT::get_username_from_token(token);
}

// ID/パスワード認証
inline bool authenticate_user(const string& username, const string& password) {
  return USER_MANAGER::user_manager.authenticate_user(username, password);
}

// JWTトークン作成
inline string create_token(const string& username) {
  return generate_token(username);
}

// JWTトークン検証（ラッパー関数）
inline bool validate_token_wrapper(const string& token) {
  return validate_token(token);
}

// APIレスポンス用の関数
crow::response login_response(const crow::request& req);
crow::response logout_response(const crow::request& req);
crow::response check_auth_response(const crow::request& req);

} // namespace AUTH 