#pragma once
#include "user_manager.hpp"
#include "headers.hpp"
#include <sstream>

using namespace std;

namespace USER_API {

// ユーザー登録API
inline crow::response register_user(const crow::request& req) {
    try {
        auto data = crow::json::load(req.body);
        string username = data["username"].s();
        string password = data["password"].s();
        string role = data["role"].s();
        string created_by = data["created_by"].s();
        
        // 毎回ユーザー存在確認
        bool is_first_user = USER_MANAGER::user_manager.is_first_user();
        
        // 初回ユーザーでない場合の権限チェック
        if (!is_first_user) {
            if (role == "admin" && !USER_MANAGER::user_manager.can_register_admin(created_by)) {
                crow::json::wvalue response;
                response["success"] = false;
                response["message"] = "管理者登録の権限がありません";
                return crow::response(403, response);
            }
            
            if (role == "user" && !USER_MANAGER::user_manager.can_register_user(created_by)) {
                crow::json::wvalue response;
                response["success"] = false;
                response["message"] = "ユーザー登録の権限がありません";
                return crow::response(403, response);
            }
        }
        
        // ユーザー登録
        if (USER_MANAGER::user_manager.add_user(username, password, role, created_by)) {
            crow::json::wvalue response;
            response["success"] = true;
            response["message"] = is_first_user ? "初回管理者アカウントが正常に登録されました" : "ユーザーが正常に登録されました";
            return crow::response(response);
        } else {
            crow::json::wvalue response;
            response["success"] = false;
            response["message"] = "ユーザー登録に失敗しました（ユーザー名が既に存在する可能性があります）";
            return crow::response(400, response);
        }
    } catch (const std::exception& e) {
        crow::json::wvalue response;
        response["success"] = false;
        response["message"] = "リクエストの処理中にエラーが発生しました";
        return crow::response(400, response);
    }
}

// ユーザー削除API
inline crow::response delete_user(const crow::request& req) {
    try {
        auto data = crow::json::load(req.body);
        string username = data["username"].s();
        string deleted_by = data["deleted_by"].s();
        
        if (USER_MANAGER::user_manager.delete_user(username, deleted_by)) {
            crow::json::wvalue response;
            response["success"] = true;
            response["message"] = "ユーザーが正常に削除されました";
            return crow::response(response);
        } else {
            crow::json::wvalue response;
            response["success"] = false;
            response["message"] = "ユーザー削除に失敗しました";
            return crow::response(400, response);
        }
    } catch (const std::exception& e) {
        crow::json::wvalue response;
        response["success"] = false;
        response["message"] = "リクエストの処理中にエラーが発生しました";
        return crow::response(400, response);
    }
}

// ユーザー昇格API
inline crow::response promote_user(const crow::request& req) {
    try {
        auto data = crow::json::load(req.body);
        string username = data["username"].s();
        string promoted_by = data["promoted_by"].s();
        
        if (USER_MANAGER::user_manager.promote_user(username, promoted_by)) {
            crow::json::wvalue response;
            response["success"] = true;
            response["message"] = "ユーザーが管理者に昇格されました";
            return crow::response(response);
        } else {
            crow::json::wvalue response;
            response["success"] = false;
            response["message"] = "ユーザー昇格に失敗しました";
            return crow::response(400, response);
        }
    } catch (const std::exception& e) {
        crow::json::wvalue response;
        response["success"] = false;
        response["message"] = "リクエストの処理中にエラーが発生しました";
        return crow::response(400, response);
    }
}

// ユーザー降格API
inline crow::response demote_user(const crow::request& req) {
    try {
        auto data = crow::json::load(req.body);
        string username = data["username"].s();
        string demoted_by = data["demoted_by"].s();
        
        if (USER_MANAGER::user_manager.demote_user(username, demoted_by)) {
            crow::json::wvalue response;
            response["success"] = true;
            response["message"] = "ユーザーが一般ユーザーに降格されました";
            return crow::response(response);
        } else {
            crow::json::wvalue response;
            response["success"] = false;
            response["message"] = "ユーザー降格に失敗しました";
            return crow::response(400, response);
        }
    } catch (const std::exception& e) {
        crow::json::wvalue response;
        response["success"] = false;
        response["message"] = "リクエストの処理中にエラーが発生しました";
        return crow::response(400, response);
    }
}

// ユーザー一覧取得API
inline crow::response get_user_list(const crow::request& req) {
    try {
        auto users = USER_MANAGER::user_manager.get_all_users();
        
        crow::json::wvalue::list user_list;
        for (const auto& user : users) {
            crow::json::wvalue user_data;
            user_data["username"] = html_escape(user.username);
            user_data["role"] = html_escape(user.role);
            user_data["created_by"] = html_escape(user.created_by);
            user_data["created_at"] = html_escape(user.created_at);
            user_data["last_login"] = html_escape(user.last_login);
            user_list.emplace_back(std::move(user_data));
        }
        
        crow::json::wvalue response;
        response["success"] = true;
        response["users"] = std::move(user_list);
        response["total"] = users.size();
        
        return crow::response(response);
    } catch (const std::exception& e) {
        crow::json::wvalue response;
        response["success"] = false;
        response["message"] = "ユーザー一覧の取得中にエラーが発生しました";
        return crow::response(400, response);
    }
}

// 初回ユーザー確認API
inline crow::response check_first_user(const crow::request& req) {
    try {
        bool is_first = USER_MANAGER::user_manager.is_first_user();
        
        crow::json::wvalue response;
        response["success"] = true;
        response["is_first_user"] = is_first;
        response["message"] = is_first ? "初回ユーザーです" : "既存ユーザーが存在します";
        
        return crow::response(response);
    } catch (const std::exception& e) {
        crow::json::wvalue response;
        response["success"] = false;
        response["message"] = "初回ユーザー確認中にエラーが発生しました";
        return crow::response(400, response);
    }
}

// ユーザー権限確認API
inline crow::response get_user_permissions(const crow::request& req) {
    try {
        auto data = crow::json::load(req.body);
        string username = data["username"].s();
        
        bool is_admin = USER_MANAGER::user_manager.is_admin(username);
        bool can_register_admin = USER_MANAGER::user_manager.can_register_admin(username);
        bool can_register_user = USER_MANAGER::user_manager.can_register_user(username);
        bool can_manage_users = USER_MANAGER::user_manager.can_manage_users(username);
        
        crow::json::wvalue response;
        response["success"] = true;
        response["username"] = username;
        response["is_admin"] = is_admin;
        response["can_register_admin"] = can_register_admin;
        response["can_register_user"] = can_register_user;
        response["can_manage_users"] = can_manage_users;
        
        return crow::response(response);
    } catch (const std::exception& e) {
        crow::json::wvalue response;
        response["success"] = false;
        response["message"] = "権限確認中にエラーが発生しました";
        return crow::response(400, response);
    }
}

} // namespace USER_API 