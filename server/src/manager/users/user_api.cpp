#include <regex>

#include <inline_helper.hpp>
#include <manager/users/api.hpp>
#include <manager/inline_helper.hpp>
#include <manager/auth/auth.hpp>
#include <manager/auth/middleware.hpp>

namespace USER_API {
using namespace std;

crow::response register_user(const crow::request& req) {
  try {
    auto data = crow::json::load(req.body);
    string username = data["username"].s();
    string password = data["password"].s();
    string role = data["role"].s();
    // 毎回ユーザー存在確認
    bool is_first_user = USER_MANAGER::user_manager.is_first_user();
    // 実行者（権限判断/監査用）はトークンから取得（クライアント入力は信用しない）
    string created_by = is_first_user ? "system" : AUTH::get_username_from_token(MIDDLEWARE::extract_token(req));
    // 初回ユーザーでない場合の権限チェック
    if (!is_first_user) {
      // 認可はミドルウェアで実施するが、created_by が空の場合は不正なので弾く
      if (created_by.empty())
        return default_response(false, "認証が必要です", 401);
    }
    // 入力バリデーション
    static const std::regex re_user("^[A-Za-z0-9]{1,32}$");
    static const std::regex re_pass("^[A-Za-z0-9_-]{10,64}$");
    if (!std::regex_match(username, re_user) || !std::regex_match(password, re_pass))
      return default_response(false, "ユーザー名/パスワードの形式が不正です", 400);

    // ユーザー登録
    if (USER_MANAGER::user_manager.add_user(username, password, role, created_by))
      return default_response(true, is_first_user ? "初回管理者アカウントが正常に登録されました" : "ユーザーが正常に登録されました");
    else return default_response(false, "ユーザー登録に失敗しました（ユーザー名が既に存在する可能性があります）", 400);
  } catch (const std::exception& e) {
    return default_response(false, "リクエストの処理中にエラーが発生しました", 400);
  }
}

crow::response delete_user(const crow::request& req) {
  try {
    auto data = crow::json::load(req.body);
    string username = data["username"].s();
    string deleted_by = AUTH::get_username_from_token(MIDDLEWARE::extract_token(req));
    if (deleted_by.empty())
      return default_response(false, "認証が必要です", 401);
    
    if (USER_MANAGER::user_manager.delete_user(username, deleted_by))
      return default_response(true, "ユーザーが正常に削除されました");
    else return default_response(false, "ユーザー削除に失敗しました", 400);
  } catch (const std::exception& e) {
    return default_response(false, "リクエストの処理中にエラーが発生しました", 400);
  }
}

crow::response promote_user(const crow::request& req) {
  try {
    auto data = crow::json::load(req.body);
    string username = data["username"].s();
    string promoted_by = AUTH::get_username_from_token(MIDDLEWARE::extract_token(req));
    if (promoted_by.empty())
      return default_response(false, "認証が必要です", 401);
    
    if (USER_MANAGER::user_manager.promote_user(username, promoted_by))
      return default_response(true, "ユーザーが管理者に昇格されました");
    else return default_response(false, "ユーザー昇格に失敗しました", 400);
  } catch (const std::exception& e) {
    return default_response(false, "リクエストの処理中にエラーが発生しました", 400);
  }
}

crow::response demote_user(const crow::request& req) {
  try {
    auto data = crow::json::load(req.body);
    string username = data["username"].s();
    string demoted_by = AUTH::get_username_from_token(MIDDLEWARE::extract_token(req));
    if (demoted_by.empty())
      return default_response(false, "認証が必要です", 401);
    
    if (USER_MANAGER::user_manager.demote_user(username, demoted_by))
      return default_response(true, "ユーザーが一般ユーザーに降格されました");
    else return default_response(false, "ユーザー降格に失敗しました", 400);
  } catch (const std::exception& e) {
    return default_response(false, "リクエストの処理中にエラーが発生しました", 400);
  }
}

crow::response get_user_list(const crow::request& req) {
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
    return default_response(false, "ユーザー一覧の取得中にエラーが発生しました", 400);
  }
}

crow::response check_first_user(const crow::request& req) {
  try {
    bool is_first = USER_MANAGER::user_manager.is_first_user();
    
    crow::json::wvalue response;
    response["success"] = true;
    response["is_first_user"] = is_first;
    response["message"] = is_first ? "初回ユーザーです" : "既存ユーザーが存在します";
    
    return crow::response(response);
  } catch (const std::exception& e) {
    return default_response(false, "初回ユーザー確認中にエラーが発生しました", 400);
  }
}

crow::response get_user_permissions(const crow::request& req) {
  try {
    string token = MIDDLEWARE::extract_token(req);
    if (token.empty() || !AUTH::validate_token_wrapper(token))
      return default_response(false, "認証が必要です", 401);
    string username = AUTH::get_username_from_token(token);
    if (username.empty())
      return default_response(false, "ユーザー情報が取得できません", 400);
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
    return default_response(false, "権限確認中にエラーが発生しました", 400);
  }
}
}//namespace USER_API