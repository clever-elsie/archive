#include <manager/auth/middleware.hpp>

namespace MIDDLEWARE {
using namespace std;

inline bool requires_auth(const string& path) {
  // 認証が不要なパス
  static const vector<string> public_paths = {
    "/",
    "/req/auth/login",
    "/req/auth/check",
    "/req/user/check_first",
    "/req/user/register",
    "/req/user/permissions"
  };
  if (path.ends_with(".html") || path.ends_with(".css") || path.ends_with(".js"))
    return false; // HTMLファイルのパス
  for (const auto& public_path : public_paths)
    if (path == public_path)
      return false; // 公開パスのチェック
  return true; // その他は認証が必要
}

inline string extract_token(const crow::request& req) {
  // Cookie から JWT を取得（唯一の正式な経路）
  std::string cookie = req.get_header_value("Cookie");
  const std::string name = "auth_token=";
  if (!cookie.empty()) {
    auto pos = cookie.find(name);
    if (pos != std::string::npos) {
      pos += name.size();
      auto end = cookie.find(';', pos);
      if (end == std::string::npos) end = cookie.size();
      return cookie.substr(pos, end - pos);
    }
  }
  return "";
}

inline bool requires_csrf_validation(const string& path, const crow::HTTPMethod& method) {
  // GETリクエストはCSRF検証不要
  if (method == crow::HTTPMethod::GET)
    return false;
  // 認証が不要なパスはCSRF検証も不要
  if (!requires_auth(path))
    return false;
  return true; // その他のPOST/PUT/DELETEリクエストはCSRF検証が必要
}

inline bool is_allowed_origin(const string& origin) {
  // 開発環境ではすべてのオリジンを許可（デバッグ用）
  if (CONFIG::params.IS_DEVELOPMENT)
    return true;
  // 本番環境ではCONFIGから読み込んだオリジンのみ
  return CONFIG::is_origin_allowed(origin);
}

inline string get_allowed_origin(const string& request_origin) {
  if (is_allowed_origin(request_origin))
    return request_origin;
  return ""; // 許可されていない場合は空文字を返す
}

void AuthMiddleware::before_handle(crow::request& req, crow::response& res, context& ctx) {
  string path = req.url;
  
  // OPTIONSリクエスト（プリフライト）の処理
  if (req.method == crow::HTTPMethod::OPTIONS) {
    // CORSプリフライトリクエストの場合は認証チェックをスキップ
    res = crow::response(200);
    res.end();
    return;
  }
  // 認証が不要なパスはスキップ
  if (!requires_auth(path)) return;
  string token = extract_token(req); // JWTトークンを抽出
  // トークンが空または無効な場合
  if (token.empty() || !AUTH::validate_token_wrapper(token)) {
    crow::json::wvalue error_response;
    error_response["error"] = "認証が必要です";
    error_response["code"] = "AUTH_REQUIRED";
    res = crow::response(401, error_response);
    res.end();
    return;
  }
}
void AuthMiddleware::after_handle(crow::request& req, crow::response& res, context& ctx) {
  // CORSヘッダーを設定
  string origin = req.get_header_value("Origin");
  string allowed_origin = get_allowed_origin(origin);
  
  if (!allowed_origin.empty()) {
    res.add_header("Access-Control-Allow-Origin", allowed_origin);
  }
  
  res.add_header("Access-Control-Allow-Methods", CONFIG::params.ALLOWED_METHODS);
  res.add_header("Access-Control-Allow-Headers", CONFIG::params.ALLOWED_HEADERS);
  res.add_header("Access-Control-Max-Age", "86400"); // 24時間
  res.add_header("Access-Control-Allow-Credentials", "true");
  
  // セキュリティヘッダーを追加
  res.add_header("X-Content-Type-Options", "nosniff");
  res.add_header("X-Frame-Options", "DENY");
  res.add_header("X-XSS-Protection", "1; mode=block");
  res.add_header("Referrer-Policy", "strict-origin-when-cross-origin");
  res.add_header("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline' https://cdnjs.cloudflare.com https://fonts.googleapis.com; style-src 'self' 'unsafe-inline' https://cdnjs.cloudflare.com https://fonts.googleapis.com; font-src 'self' https://fonts.gstatic.com; img-src 'self' data:; connect-src 'self'");
}
}//namespace MIDDLEWARE