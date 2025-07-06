#pragma once
#include "headers.hpp"
#include "config.hpp"
#include "user_manager.hpp"
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace AUTH {
using namespace std;
using namespace std::chrono;
inline random_device rd;
inline mt19937_64 gen(rd());
// 前方宣言
inline void cleanup_expired_sessions();

// セッションID生成
inline string generate_session_id() {
    uniform_int_distribution<uint64_t> dis;
    uint64_t random_value = dis(gen);
    
    stringstream ss;
    ss << hex << setw(16) << setfill('0') << random_value;
    return ss.str();
}

// CSRFトークン生成
inline string generate_csrf_token() {
    uniform_int_distribution<uint64_t> dis;
    uint64_t random_value1 = dis(gen);
    uint64_t random_value2 = dis(gen);
    
    stringstream ss;
    ss << hex << setw(16) << setfill('0') << random_value1;
    ss << hex << setw(16) << setfill('0') << random_value2;
    return ss.str();
}


// セッション情報を格納する構造体
struct Session {
    string session_id;
    string username;
    string csrf_token;
    time_point<system_clock> created_at;
    time_point<system_clock> last_access;
    
    Session(const string& id, const string& user) : session_id(id), username(user), csrf_token(generate_csrf_token()), created_at(system_clock::now()), last_access(system_clock::now()) {}
    
    bool is_expired() const {
        auto now = system_clock::now();
        auto duration = duration_cast<minutes>(now - last_access);
        return duration.count() > CONFIG::params.SESSION_TIMEOUT_MINUTES;
    }
    
    void update_access() {
        last_access = system_clock::now();
    }
};

// 認証設定（新しいユーザー管理システムを使用）
inline map<string, Session> active_sessions;
inline mutex session_mutex;

// ID/パスワード認証
inline bool authenticate_user(const string& username, const string& password) {
    return USER_MANAGER::user_manager.authenticate_user(username, password);
}

// セッション作成
inline string create_session(const string& username) {
    lock_guard<mutex> lock(session_mutex);
    
    // 期限切れセッションをクリーンアップ
    cleanup_expired_sessions();
    
    string session_id = generate_session_id();
    active_sessions.emplace(session_id, Session(session_id, username));
    
    return session_id;
}

// セッション検証
inline bool validate_session(const string& session_id) {
    lock_guard<mutex> lock(session_mutex);
    
    auto it = active_sessions.find(session_id);
    if (it == active_sessions.end()) {
        return false;
    }
    
    if (it->second.is_expired()) {
        active_sessions.erase(it);
        return false;
    }
    
    it->second.update_access();
    return true;
}

// CSRFトークン検証
inline bool validate_csrf_token(const string& session_id, const string& csrf_token) {
    lock_guard<mutex> lock(session_mutex);
    
    auto it = active_sessions.find(session_id);
    if (it == active_sessions.end()) {
        return false;
    }
    
    return it->second.csrf_token == csrf_token;
}

// セッション削除
inline void remove_session(const string& session_id) {
    lock_guard<mutex> lock(session_mutex);
    active_sessions.erase(session_id);
}

// 期限切れセッションのクリーンアップ
inline void cleanup_expired_sessions() {
    auto it = active_sessions.begin();
    while (it != active_sessions.end()) {
        if (it->second.is_expired()) {
            it = active_sessions.erase(it);
        } else {
            ++it;
        }
    }
}

// セッションからユーザー名を取得
inline string get_username_from_session(const string& session_id) {
    lock_guard<mutex> lock(session_mutex);
    auto it = active_sessions.find(session_id);
    if (it != active_sessions.end()) {
        return it->second.username;
    }
    return "";
}

// セッションからCSRFトークンを取得
inline string get_csrf_token_from_session(const string& session_id) {
    lock_guard<mutex> lock(session_mutex);
    auto it = active_sessions.find(session_id);
    if (it != active_sessions.end()) {
        return it->second.csrf_token;
    }
    return "";
}

// APIレスポンス用の関数
inline crow::response login_response(const crow::request& req) {
    try {
        auto data = crow::json::load(req.body);
        string username = data["username"].s();
        string password = data["password"].s();
        
        if (authenticate_user(username, password)) {
            string session_id = create_session(username);
            string csrf_token = get_csrf_token_from_session(session_id);
            
            crow::json::wvalue response;
            response["success"] = true;
            response["session_id"] = session_id;
            response["csrf_token"] = csrf_token;
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
        auto data = crow::json::load(req.body);
        string session_id = data["session_id"].s();
        
        remove_session(session_id);
        
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
        string session_id = data["session_id"].s();
        
        bool is_valid = validate_session(session_id);
        
        crow::json::wvalue response;
        response["authenticated"] = is_valid;
        
        if (is_valid) {
            auto it = active_sessions.find(session_id);
            if (it != active_sessions.end()) {
                response["username"] = it->second.username;
            }
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