#include <manager/auth/middleware.hpp>

#include <algorithm>
#include <iostream>
#include <string_view>

namespace {

std::string log_safe(std::string value) {
  constexpr std::size_t MAX_LOG_LENGTH = 512;
  if (value.size() > MAX_LOG_LENGTH)
    value.resize(MAX_LOG_LENGTH);
  for (char& character : value) {
    const auto byte = static_cast<unsigned char>(character);
    if (byte < 0x20 || byte == 0x7f)
      character = '?';
  }
  return value;
}

} // namespace

namespace MIDDLEWARE {

namespace {

bool safe_method(const crow::HTTPMethod method) {
  return method == crow::HTTPMethod::GET ||
         method == crow::HTTPMethod::HEAD ||
         method == crow::HTTPMethod::OPTIONS;
}

bool public_static_path(std::string_view path) {
  return path == "/" || path == "/index.html" || path == "/memo.html" ||
         path == "/viewer.html" || path == "/user_register.html" ||
         path.starts_with("/static/");
}

bool origin_header_is_allowed(const crow::request& req) {
  const std::string origin = req.get_header_value("Origin");
  if (!origin.empty())
    return is_allowed_origin(origin);
  const std::string referer = req.get_header_value("Referer");
  if (referer.empty())
    return true;
  const auto scheme_end = referer.find("://");
  if (scheme_end == std::string::npos)
    return false;
  const auto authority_end = referer.find('/', scheme_end + 3);
  const auto origin_end =
      authority_end == std::string::npos ? referer.size() : authority_end;
  return is_allowed_origin(referer.substr(0, origin_end));
}

} // namespace

bool requires_auth(
    const std::string& path,
    const crow::HTTPMethod&) {
  if (public_static_path(path))
    return false;
  if (path == "/req/auth/login" ||
      path == "/req/auth/logout" ||
      path == "/req/auth/check" ||
      path == "/req/user/check_first" ||
      path == "/req/user/register")
    return false;
  return true;
}

std::string extract_token(const crow::request& req) {
  return AUTH::extract_cookie(req, "auth_token");
}

bool requires_csrf_validation(
    const std::string& path,
    const crow::HTTPMethod& method) {
  if (safe_method(method))
    return false;
  // Login has no pre-existing authenticated state. Origin validation is still
  // performed by the middleware, while the CSRF token starts at check/login.
  if (path == "/req/auth/login" || path == "/req/auth/check")
    return false;
  return true;
}

bool is_allowed_origin(const std::string& origin) {
  if (origin.empty())
    return true;
  return CONFIG::is_origin_allowed(origin);
}

std::string get_allowed_origin(const std::string& request_origin) {
  return is_allowed_origin(request_origin) ? request_origin : std::string();
}

void AuthMiddleware::before_handle(
    crow::request& req,
    crow::response& res,
    context& ctx) {
  const std::string path = req.url;
  if (req.method == crow::HTTPMethod::OPTIONS) {
    res.code = 204;
    res.end();
    return;
  }

  if (!safe_method(req.method) && !origin_header_is_allowed(req)) {
    std::cerr << "AuthMiddleware: rejected origin for " << req.url
              << ": origin='"
              << log_safe(req.get_header_value("Origin"))
              << "', referer='"
              << log_safe(req.get_header_value("Referer")) << "'"
              << std::endl;
    crow::json::wvalue body;
    body["success"] = false;
    body["code"] = "ORIGIN_NOT_ALLOWED";
    body["message"] = "許可されていないOriginです";
    body["error"] = "許可されていないOriginです";
    body["data"] = nullptr;
    res = crow::response(403, std::move(body));
    res.end();
    return;
  }

  // 有効なセッションが存在しないログアウトは、期限切れ・失効済み
  // Cookieの掃除も含めて冪等に成功させる。現在有効なセッションを
  // ログアウトさせる場合だけCSRF検証を要求する。
  const bool unauthenticated_logout =
      path == "/req/auth/logout" &&
      !AUTH::principal_from_request(req).has_value();
  if (requires_csrf_validation(path, req.method) &&
      !unauthenticated_logout &&
      !AUTH::validate_csrf_token(req)) {
    crow::json::wvalue body;
    body["success"] = false;
    body["code"] = "CSRF_INVALID";
    body["message"] = "CSRF tokenが不正です";
    body["error"] = "CSRF tokenが不正です";
    body["data"] = nullptr;
    res = crow::response(403, std::move(body));
    res.end();
    return;
  }

  if (!requires_auth(path, req.method))
    return;

  ctx.principal = AUTH::principal_from_request(req);
  if (!ctx.principal) {
    crow::json::wvalue body;
    body["success"] = false;
    body["code"] = "AUTH_REQUIRED";
    body["message"] = "認証が必要です";
    body["error"] = "認証が必要です";
    body["data"] = nullptr;
    res = crow::response(401, std::move(body));
    res.end();
  }
}

void AuthMiddleware::after_handle(
    crow::request& req,
    crow::response& res,
    context&) {
  const std::string origin = req.get_header_value("Origin");
  const std::string allowed_origin = get_allowed_origin(origin);
  if (!allowed_origin.empty()) {
    res.add_header("Access-Control-Allow-Origin", allowed_origin);
    res.add_header("Vary", "Origin");
  }
  res.add_header("Access-Control-Allow-Methods", CONFIG::params.ALLOWED_METHODS);
  res.add_header("Access-Control-Allow-Headers", CONFIG::params.ALLOWED_HEADERS);
  res.add_header("Access-Control-Max-Age", "86400");
  res.add_header("Access-Control-Allow-Credentials", "true");
  res.add_header("X-Content-Type-Options", "nosniff");
  res.add_header("X-Frame-Options", "DENY");
  res.add_header("Referrer-Policy", "strict-origin-when-cross-origin");
  res.add_header(
      "Content-Security-Policy",
      "default-src 'self'; script-src 'self'; style-src 'self'; "
      "font-src 'self'; img-src 'self' data:; connect-src 'self'");
}

} // namespace MIDDLEWARE
