#pragma once
#include "../../headers.hpp"
#include "auth.hpp"
#include "../config.hpp"
#include <string>
#include <crow.h>
#include <vector>

namespace MIDDLEWARE {
using namespace std;

// 認証が必要かどうかを判定
inline bool requires_auth(const string& path) {
    // 認証が不要なパス
    static const vector<string> public_paths = {
        "/",
        "/req/auth/login",
        "/req/auth/check",
        "/req/user/check_first",
        "/req/user/register"
    };
    
    // HTMLファイルのパス
    if (path.ends_with(".html") || path.ends_with(".css") || path.ends_with(".js")) {
        return false;
    }
    
    // 公開パスのチェック
    for (const auto& public_path : public_paths)
        if (path == public_path)
            return false;
    
    // その他は認証が必要
    return true;
}

// リクエストからセッションIDを抽出
inline string extract_session_id(const crow::request& req) {
    // ヘッダーからセッションIDを取得
    auto session_header = req.get_header_value("X-Session-ID");
    if (!session_header.empty()) {
        return session_header;
    }
    
    // JSONボディからセッションIDを取得（POSTリクエストの場合）
    if (req.method == crow::HTTPMethod::POST && !req.body.empty()) {
        try {
            auto data = crow::json::load(req.body);
            if (data.has("session_id")) {
                return data["session_id"].s();
            }
        } catch (...) {
            // JSONパースエラーは無視
        }
    }
    
    return "";
}

// リクエストからCSRFトークンを抽出
inline string extract_csrf_token(const crow::request& req) {
    // ヘッダーからCSRFトークンを取得
    auto csrf_header = req.get_header_value("X-CSRF-Token");
    if (!csrf_header.empty()) {
        return csrf_header;
    }
    
    // JSONボディからCSRFトークンを取得（POSTリクエストの場合）
    if (req.method == crow::HTTPMethod::POST && !req.body.empty()) {
        try {
            auto data = crow::json::load(req.body);
            if (data.has("csrf_token")) {
                return data["csrf_token"].s();
            }
        } catch (...) {
            // JSONパースエラーは無視
        }
    }
    
    return "";
}

// CSRF検証が必要かどうかを判定
inline bool requires_csrf_validation(const string& path, const crow::HTTPMethod& method) {
    // GETリクエストはCSRF検証不要
    if (method == crow::HTTPMethod::GET) {
        return false;
    }
    
    // 認証が不要なパスはCSRF検証も不要
    if (!requires_auth(path)) {
        return false;
    }
    
    // その他のPOST/PUT/DELETEリクエストはCSRF検証が必要
    return true;
}

// CORS検証
inline bool is_allowed_origin(const string& origin) {
    // 開発環境ではすべてのオリジンを許可（デバッグ用）
    if (CONFIG::params.IS_DEVELOPMENT) {
        return true;
    }
    
    // 本番環境ではCONFIGから読み込んだオリジンのみ
    return CONFIG::is_origin_allowed(origin);
}

// 許可されたオリジンを取得（CORSヘッダー用）
inline string get_allowed_origin(const string& request_origin) {
    if (is_allowed_origin(request_origin)) {
        return request_origin;
    }
    return ""; // 許可されていない場合は空文字を返す
}

// 認証ミドルウェアクラス
struct AuthMiddleware {
    struct context {};
    
    inline void before_handle(crow::request& req, crow::response& res, context& ctx) {
        string path = req.url;
        
        // OPTIONSリクエスト（プリフライト）の処理
        if (req.method == crow::HTTPMethod::OPTIONS) {
            // CORSプリフライトリクエストの場合は認証チェックをスキップ
            res = crow::response(200);
            res.end();
            return;
        }
        
        // 認証が不要なパスはスキップ
        if (!requires_auth(path)) {
            return;
        }
        
        // セッションIDを抽出
        string session_id = extract_session_id(req);
        
        // セッションIDが空または無効な場合
        if (session_id.empty() || !AUTH::validate_session(session_id)) {
            crow::json::wvalue error_response;
            error_response["error"] = "認証が必要です";
            error_response["code"] = "AUTH_REQUIRED";
            
            res = crow::response(401, error_response);
            res.end();
            return;
        }
        
        // CSRF検証が必要な場合
        if (requires_csrf_validation(path, req.method)) {
            string csrf_token = extract_csrf_token(req);
            
            // CSRFトークンが空または無効な場合
            if (csrf_token.empty() || !AUTH::validate_csrf_token(session_id, csrf_token)) {
                crow::json::wvalue error_response;
                error_response["error"] = "CSRFトークンが無効です";
                error_response["code"] = "CSRF_INVALID";
                
                res = crow::response(403, error_response);
                res.end();
                return;
            }
        }
    }
    
    inline void after_handle(crow::request& req, crow::response& res, context& ctx) {
        // CORSヘッダーを設定
        string origin = req.get_header_value("Origin");
        string allowed_origin = get_allowed_origin(origin);
        
        if (!allowed_origin.empty()) {
            res.add_header("Access-Control-Allow-Origin", allowed_origin);
        }
        
        res.add_header("Access-Control-Allow-Methods", CONFIG::ALLOWED_METHODS);
        res.add_header("Access-Control-Allow-Headers", CONFIG::ALLOWED_HEADERS);
        res.add_header("Access-Control-Max-Age", "86400"); // 24時間
        res.add_header("Access-Control-Allow-Credentials", "true");
        
        // セキュリティヘッダーを追加
        res.add_header("X-Content-Type-Options", "nosniff");
        res.add_header("X-Frame-Options", "DENY");
        res.add_header("X-XSS-Protection", "1; mode=block");
        res.add_header("Referrer-Policy", "strict-origin-when-cross-origin");
        res.add_header("Content-Security-Policy", "default-src 'self'; script-src 'self' 'unsafe-inline' https://cdnjs.cloudflare.com https://fonts.googleapis.com; style-src 'self' 'unsafe-inline' https://cdnjs.cloudflare.com https://fonts.googleapis.com; font-src 'self' https://fonts.gstatic.com; img-src 'self' data:; connect-src 'self'");
    }
};

} // namespace MIDDLEWARE 