#include <regex>

#include <manager/auth/auth.hpp>
#include <manager/inline_helper.hpp>

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
      crow::json::wvalue body;
      body["success"] = true;
      body["username"] = username;
      body["message"] = "ログインに成功しました";

      crow::response res(body);
      // HttpOnly/SameSite=Lax なクッキーに JWT を格納
      std::string cookie = "auth_token=" + token + "; Path=/; HttpOnly; SameSite=Lax";
      if (!CONFIG::params.IS_DEVELOPMENT) {
        cookie += "; Secure";
      }
      res.add_header("Set-Cookie", cookie);
      return res;
    } else
      return default_response(false, "ユーザー名またはパスワードが正しくありません", 401);
  } catch (const exception& e) {
    return default_response(false, "リクエストの処理中にエラーが発生しました", 400);
  }
}

crow::response logout_response(const crow::request& req) {
  try {
    crow::json::wvalue body;
    body["success"] = true;
    body["message"] = "ログアウトしました";
    crow::response res(body);
    // クッキーを無効化
    std::string cookie = "auth_token=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0";
    if (!CONFIG::params.IS_DEVELOPMENT) {
      cookie += "; Secure";
    }
    res.add_header("Set-Cookie", cookie);
    return res;
  } catch (const exception& e) {
    return default_response(false, "リクエストの処理中にエラーが発生しました", 400);
  }
}

static std::string extract_token_from_cookie(const crow::request& req) {
  std::string cookie = req.get_header_value("Cookie");
  if (cookie.empty()) return "";
  const std::string name = "auth_token=";
  auto pos = cookie.find(name);
  if (pos == std::string::npos) return "";
  pos += name.size();
  auto end = cookie.find(';', pos);
  if (end == std::string::npos) end = cookie.size();
  return cookie.substr(pos, end - pos);
}

crow::response check_auth_response(const crow::request& req) {
  try {
    std::string token = extract_token_from_cookie(req);
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