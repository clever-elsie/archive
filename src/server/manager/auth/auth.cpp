#include "manager/auth/auth.hpp"
#include "manager/inline_helper.hpp"
#include <regex>

namespace AUTH {
using namespace std;

crow::response login_response(const crow::request& req) {
  try {
    auto data = crow::json::load(req.body);
    string username = data["username"].s();
    string password = data["password"].s();
    static const std::regex re_user("^[A-Za-z0-9]{1,32}$");
    static const std::regex re_pass("^[A-Za-z0-9_-]{10,64}$");
    if (!std::regex_match(username, re_user) || !std::regex_match(password, re_pass))
      return default_response(false, "ユーザー名/パスワードの形式が不正です", 400);
    if (authenticate_user(username, password)) {
      string token = create_token(username);
      crow::json::wvalue response;
      response["success"] = true;
      response["token"] = token;
      response["username"] = username;
      response["message"] = "ログインに成功しました";
      return crow::response(response);
    } else
      return default_response(false, "ユーザー名またはパスワードが正しくありません", 401);
  } catch (const exception& e) {
    return default_response(false, "リクエストの処理中にエラーが発生しました", 400);
  }
}

crow::response logout_response(const crow::request& req) {
  try {
    // JWTトークン方式ではサーバーサイドでセッションを管理しないため、
    // クライアントサイドでトークンを削除するだけで良い
    return default_response(true, "ログアウトしました");
  } catch (const exception& e) {
    return default_response(false, "リクエストの処理中にエラーが発生しました", 400);
  }
}

crow::response check_auth_response(const crow::request& req) {
  try {
    auto data = crow::json::load(req.body);
    string token = data["token"].s();
    bool is_valid = validate_token(token);
    crow::json::wvalue response;
    response["authenticated"] = is_valid;
    if (is_valid) {
      response["username"] = get_username_from_token(token);
      response["message"] = "認証済み";
    } else {
      response["message"] = "認証が必要です";
    }
    return crow::response(response);
  } catch (const exception& e) {
    crow::json::wvalue response;
    response["authenticated"] = false;
    response["message"] = "リクエストの処理中にエラーが発生しました";
    return crow::response(400, response);
  }
}
}//namespace AUTH