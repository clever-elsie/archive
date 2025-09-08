#pragma once
#include "../../headers.hpp"
#include "../config.hpp"
#include "../users/user_manager.hpp"
#include "jwt.hpp"
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <regex>

namespace AUTH {
using namespace std;
using namespace std::chrono;

// JWTトークン生成
inline string generate_token(const string& username) {
    return JWT::generate_token(username, CONFIG::params.JWT_SECRET_KEY);
}

// JWTトークン検証
inline bool validate_token(const string& token) {
    if (token.empty()) {
        return false;
    }
    
    // トークンの署名を検証
    if (!JWT::verify_token(token, CONFIG::params.JWT_SECRET_KEY)) {
        return false;
    }
    
    // トークンの有効期限をチェック
    if (JWT::is_token_expired(token)) {
        return false;
    }
    
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
inline crow::response login_response(const crow::request& req) {
    try {
        auto data = crow::json::load(req.body);
        string username = data["username"].s();
        string password = data["password"].s();
        
        static const std::regex re_user("^[A-Za-z0-9]{1,32}$");
        static const std::regex re_pass("^[A-Za-z0-9_-]{10,64}$");
        if (!std::regex_match(username, re_user) || !std::regex_match(password, re_pass)) {
            crow::json::wvalue response;
            response["success"] = false;
            response["message"] = "ユーザー名/パスワードの形式が不正です";
            return crow::response(400, response);
        }

        if (authenticate_user(username, password)) {
            string token = create_token(username);
            
            crow::json::wvalue response;
            response["success"] = true;
            response["token"] = token;
            response["username"] = username;
            response["message"] = "ログインに成功しました";
            
            return crow::response(response);
        } else {
            crow::json::wvalue response;
            response["success"] = false;
            response["message"] = "ユーザー名またはパスワードが正しくありません";
            
            return crow::response(401, response);
        }
    } catch (const exception& e) {
        crow::json::wvalue response;
        response["success"] = false;
        response["message"] = "リクエストの処理中にエラーが発生しました";
        
        return crow::response(400, response);
    }
}

inline crow::response logout_response(const crow::request& req) {
    try {
        // JWTトークン方式ではサーバーサイドでセッションを管理しないため、
        // クライアントサイドでトークンを削除するだけで良い
        crow::json::wvalue response;
        response["success"] = true;
        response["message"] = "ログアウトしました";
        
        return crow::response(response);
    } catch (const exception& e) {
        crow::json::wvalue response;
        response["success"] = false;
        response["message"] = "リクエストの処理中にエラーが発生しました";
        
        return crow::response(400, response);
    }
}

inline crow::response check_auth_response(const crow::request& req) {
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

} // namespace AUTH 