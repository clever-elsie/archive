#include <manager/auth/auth.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <regex>
#include <string_view>

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <manager/auth/login_guard.hpp>
#include <manager/config.hpp>
#include <manager/users/manager.hpp>

namespace AUTH {

namespace {

constexpr std::string_view AUTH_COOKIE = "auth_token";
constexpr std::string_view CSRF_COOKIE = "csrf_token";
constexpr std::string_view CSRF_HEADER = "X-CSRF-Token";
constexpr std::string_view ISSUER = "home-server";
constexpr std::string_view AUDIENCE = "home-server-users";

crow::response response(
    bool success,
    int status,
    const std::string& code,
    const std::string& message) {
  crow::json::wvalue body;
  body["success"] = success;
  body["code"] = code;
  body["message"] = message;
  body["data"] = nullptr;
  if (!success)
    body["error"] = message;
  return crow::response(status, std::move(body));
}

std::string cookie_attributes(std::uint64_t max_age, bool http_only) {
  std::string attributes =
      "; Path=/; SameSite=Lax; Max-Age=" + std::to_string(max_age);
  if (http_only)
    attributes += "; HttpOnly";
  if (!CONFIG::params.IS_DEVELOPMENT)
    attributes += "; Secure";
  return attributes;
}

void add_cookie(
    crow::response& response,
    std::string_view name,
    const std::string& value,
    std::uint64_t max_age,
    bool http_only) {
  response.add_header(
      "Set-Cookie",
      std::string(name) + "=" + value +
          cookie_attributes(max_age, http_only));
}

bool constant_time_equal(
    const std::string& left,
    const std::string& right) {
  return left.size() == right.size() && !left.empty() &&
         CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}

bool valid_credentials_format(
    const std::string& username,
    const std::string& password) {
  static const std::regex username_re("^[A-Za-z0-9]{1,32}$");
  static const std::regex password_re("^[A-Za-z0-9_-]{10,64}$");
  return std::regex_match(username, username_re) &&
         std::regex_match(password, password_re);
}

std::string remote_ip(const crow::request& request) {
  // nginxの設定でX-Real-IPは接続元アドレスに置き換えられるため、
  // リバースプロキシ経由ではこれを優先する。アプリケーションを直接
  // 公開する場合は、このヘッダーを外部から注入できない構成にする。
  const std::string forwarded = request.get_header_value("X-Real-IP");
  if (!forwarded.empty())
    return forwarded;
  return request.remote_ip_address.empty()
             ? std::string("unknown")
             : request.remote_ip_address;
}

} // namespace

std::string extract_cookie(
    const crow::request& req,
    std::string_view name) {
  const std::string cookie_header = req.get_header_value("Cookie");
  std::size_t start = 0;
  while (start < cookie_header.size()) {
    const std::size_t end = cookie_header.find(';', start);
    const std::size_t length =
        end == std::string::npos ? cookie_header.size() - start : end - start;
    std::string_view item(cookie_header.data() + start, length);
    while (!item.empty() && item.front() == ' ')
      item.remove_prefix(1);
    const std::size_t separator = item.find('=');
    if (separator != std::string::npos &&
        item.substr(0, separator) == name)
      return std::string(item.substr(separator + 1));
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return {};
}

std::string create_csrf_token() {
  std::array<unsigned char, 32> bytes{};
  if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
    return {};
  constexpr char hex[] = "0123456789abcdef";
  std::string token;
  token.reserve(bytes.size() * 2);
  for (const unsigned char value : bytes) {
    token.push_back(hex[value >> 4]);
    token.push_back(hex[value & 0x0f]);
  }
  return token;
}

void add_csrf_cookie(crow::response& response, const std::string& token) {
  if (!token.empty())
    add_cookie(
        response,
        CSRF_COOKIE,
        token,
        static_cast<std::uint64_t>(
            std::max(0, CONFIG::params.SESSION_TIMEOUT_MINUTES)) *
            60,
        false);
}

void clear_auth_cookies(crow::response& response) {
  std::string attributes = "; Path=/; SameSite=Lax; Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT";
  if (!CONFIG::params.IS_DEVELOPMENT)
    attributes += "; Secure";
  response.add_header("Set-Cookie", "auth_token=" + attributes + "; HttpOnly");
  response.add_header("Set-Cookie", "csrf_token=" + attributes);
}

std::string generate_token(const std::string& username) {
  const auto generation =
      USER_MANAGER::get_user_manager().get_session_generation(username);
  if (!generation)
    return {};
  return JWT::generate_token(
      username, CONFIG::params.JWT_SECRET_KEY, *generation);
}

bool validate_token(const std::string& token) {
  if (!JWT::verify_token(token, CONFIG::params.JWT_SECRET_KEY))
    return false;
  const JWT::JWTPayload payload = JWT::decode_payload(token);
  if (!payload.parsed || payload.iss != ISSUER ||
      payload.aud != AUDIENCE || payload.sub.empty() ||
      payload.jti.empty() || payload.exp <= payload.iat)
    return false;
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  constexpr std::int64_t CLOCK_SKEW_SECONDS = 60;
  if (payload.iat > now + CLOCK_SKEW_SECONDS || now >= payload.exp)
    return false;
  if (!USER_MANAGER::get_user_manager().session_matches(
          payload.sub, payload.session_generation))
    return false;
  return true;
}

std::string get_username_from_token(const std::string& token) {
  return validate_token(token) ? JWT::get_username_from_token(token)
                               : std::string();
}

bool authenticate_user(
    const std::string& username,
    const std::string& password) {
  return USER_MANAGER::get_user_manager().authenticate_user(username, password);
}

std::optional<Principal> principal_from_token(const std::string& token) {
  if (!validate_token(token))
    return std::nullopt;
  const JWT::JWTPayload payload = JWT::decode_payload(token);
  const std::string role =
      USER_MANAGER::get_user_manager().get_user_role(payload.sub);
  if (role.empty())
    return std::nullopt;
  return Principal{payload.sub, role, payload.session_generation};
}

std::optional<Principal> principal_from_request(const crow::request& req) {
  return principal_from_token(extract_cookie(req, AUTH_COOKIE));
}

bool validate_csrf_token(const crow::request& req) {
  const std::string cookie = extract_cookie(req, CSRF_COOKIE);
  const std::string header = req.get_header_value(CSRF_HEADER.data());
  return constant_time_equal(cookie, header);
}

crow::response login_response(const crow::request& req) {
  try {
    const auto data = crow::json::load(req.body);
    if (!data || !data.has("username") || !data.has("password") ||
        data["username"].t() != crow::json::type::String ||
        data["password"].t() != crow::json::type::String)
      return response(false, 400, "BAD_REQUEST", "リクエストの形式が不正です");

    const std::string username = data["username"].s();
    const std::string password = data["password"].s();
    if (!valid_credentials_format(username, password))
      return response(
          false, 400, "INVALID_CREDENTIALS_FORMAT",
          "ユーザー名/パスワードの形式が不正です");

    std::uint64_t retry_after = 0;
    if (!login_attempt_guard().allowed(username, remote_ip(req), &retry_after)) {
      crow::response blocked =
          response(false, 429, "LOGIN_TEMPORARILY_BLOCKED", "しばらく待ってから再試行してください");
      blocked.add_header("Retry-After", std::to_string(retry_after));
      return blocked;
    }

    if (!authenticate_user(username, password)) {
      login_attempt_guard().record_failure(username, remote_ip(req));
      return response(
          false, 401, "INVALID_CREDENTIALS",
          "ユーザー名またはパスワードが正しくありません");
    }
    login_attempt_guard().record_success(username, remote_ip(req));

    const std::string token = generate_token(username);
    const std::string csrf = create_csrf_token();
    if (token.empty() || csrf.empty())
      return response(false, 500, "AUTHENTICATION_UNAVAILABLE", "認証を開始できません");

    crow::json::wvalue body;
    body["success"] = true;
    body["code"] = "LOGIN_SUCCEEDED";
    body["message"] = "ログインに成功しました";
    body["username"] = username;
    const std::string role =
        USER_MANAGER::get_user_manager().get_user_role(username);
    body["role"] = role;
    crow::json::wvalue data_body;
    data_body["username"] = username;
    data_body["role"] = role;
    body["data"] = std::move(data_body);
    crow::response result(200, std::move(body));
    add_cookie(
        result,
        AUTH_COOKIE,
        token,
        static_cast<std::uint64_t>(
            std::max(0, CONFIG::params.SESSION_TIMEOUT_MINUTES)) *
            60,
        true);
    add_csrf_cookie(result, csrf);
    return result;
  } catch (...) {
    return response(false, 400, "BAD_REQUEST", "リクエストの処理中にエラーが発生しました");
  }
}

crow::response logout_response(const crow::request& req) {
  const std::string token = extract_cookie(req, AUTH_COOKIE);
  // 期限切れ・既に失効したCookieでもログアウト自体は冪等に成功させる。
  // 現在有効なtokenだけをセッション世代の更新対象にすることで、古い
  // 署名済みtokenを持つ第三者が全セッションを失効させることを防ぐ。
  if (const auto principal = principal_from_token(token)) {
    const auto result =
        USER_MANAGER::get_user_manager().invalidate_all_sessions(principal->username);
    if (result != USER_MANAGER::MutationResult::success) {
      crow::response failure =
          response(false, 503, "SESSION_INVALIDATION_FAILED", "ログアウトを完了できません");
      clear_auth_cookies(failure);
      return failure;
    }
  }
  crow::response result =
      response(true, 200, "LOGOUT_SUCCEEDED", "ログアウトしました");
  clear_auth_cookies(result);
  return result;
}

crow::response check_auth_response(const crow::request& req) {
  const auto principal = principal_from_request(req);
  crow::json::wvalue body;
  body["success"] = true;
  body["authenticated"] = principal.has_value();
  body["code"] = principal ? "AUTHENTICATED" : "UNAUTHENTICATED";
  body["message"] = principal ? "認証済み" : "認証が必要です";
  if (principal) {
    body["username"] = principal->username;
    body["role"] = principal->role;
    crow::json::wvalue data;
    data["username"] = principal->username;
    data["role"] = principal->role;
    body["data"] = std::move(data);
  } else {
    body["data"] = nullptr;
  }
  crow::response result(200, std::move(body));
  if (extract_cookie(req, CSRF_COOKIE).empty())
    add_csrf_cookie(result, create_csrf_token());
  return result;
}

} // namespace AUTH
