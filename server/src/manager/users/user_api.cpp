#include <manager/users/api.hpp>

#include <regex>

#include <manager/auth/auth.hpp>
#include <manager/inline_helper.hpp>

namespace USER_API {

namespace {

const std::regex USERNAME_RE("^[A-Za-z0-9]{1,32}$");
const std::regex PASSWORD_RE("^[A-Za-z0-9_-]{10,64}$");

crow::response result_response(
    USER_MANAGER::MutationResult result,
    const std::string& success_message,
    const std::string& failure_message) {
  if (result == USER_MANAGER::MutationResult::success)
    return default_response(true, std::string(success_message));

  int status = 400;
  const char* code = "USER_OPERATION_FAILED";
  switch (result) {
    case USER_MANAGER::MutationResult::invalid_input:
      status = 400; code = "INVALID_INPUT"; break;
    case USER_MANAGER::MutationResult::not_initialized:
      status = 503; code = "USER_STORE_UNAVAILABLE"; break;
    case USER_MANAGER::MutationResult::invalid_actor:
      status = 401; code = "AUTH_REQUIRED"; break;
    case USER_MANAGER::MutationResult::forbidden:
      status = 403; code = "FORBIDDEN"; break;
    case USER_MANAGER::MutationResult::not_found:
      status = 404; code = "USER_NOT_FOUND"; break;
    case USER_MANAGER::MutationResult::already_exists:
      status = 409; code = "USER_ALREADY_EXISTS"; break;
    case USER_MANAGER::MutationResult::last_admin:
      status = 409; code = "LAST_ADMIN_REQUIRED"; break;
    case USER_MANAGER::MutationResult::save_failed:
      status = 503; code = "USER_STORE_SAVE_FAILED"; break;
    case USER_MANAGER::MutationResult::invalid_credentials:
      status = 401; code = "INVALID_CREDENTIALS"; break;
    case USER_MANAGER::MutationResult::invalid_role:
      status = 400; code = "INVALID_ROLE"; break;
    case USER_MANAGER::MutationResult::crypto_failed:
      status = 500; code = "CRYPTO_UNAVAILABLE"; break;
    default:
      break;
  }
  crow::json::wvalue body;
  body["success"] = false;
  body["code"] = code;
  body["message"] = failure_message;
  body["error"] = failure_message;
  body["data"] = nullptr;
  return crow::response(status, std::move(body));
}

std::optional<crow::json::rvalue> body_json(const crow::request& req) {
  auto json = crow::json::load(req.body);
  if (!json)
    return std::nullopt;
  return std::move(json);
}

std::optional<std::string> string_field(
    const crow::json::rvalue& json,
    const char* name) {
  if (!json.has(name) || json[name].t() != crow::json::type::String)
    return std::nullopt;
  return json[name].s();
}

} // namespace

crow::response register_user(const crow::request& req) {
  const auto json = body_json(req);
  if (!json)
    return default_response(false, "リクエストの形式が不正です", 400);
  const auto username = string_field(*json, "username");
  const auto password = string_field(*json, "password");
  if (!username || !password ||
      !std::regex_match(*username, USERNAME_RE) ||
      !std::regex_match(*password, PASSWORD_RE))
    return default_response(false, "ユーザー名/パスワードの形式が不正です", 400);

  const bool first_user = USER_MANAGER::get_user_manager().is_first_user();
  std::string created_by = "system";
  std::string role = "admin";
  if (!first_user) {
    const auto principal = AUTH::principal_from_request(req);
    if (!principal || !principal->is_admin())
      return default_response(false, "管理者権限が必要です", 403);
    created_by = principal->username;
    if (const auto requested_role = string_field(*json, "role"))
      role = *requested_role;
    else
      return default_response(false, "roleは必須です", 400);
  }

  const auto result = USER_MANAGER::get_user_manager().add_user(
      *username, *password, role, created_by);
  return result_response(
      result,
      first_user ? "初回管理者アカウントが正常に登録されました"
                 : "ユーザーが正常に登録されました",
      "ユーザー登録に失敗しました");
}

crow::response delete_user(const crow::request& req) {
  const auto json = body_json(req);
  const auto principal = AUTH::principal_from_request(req);
  if (!principal)
    return default_response(false, "認証が必要です", 401);
  if (!json)
    return default_response(false, "リクエストの形式が不正です", 400);
  const auto username = string_field(*json, "username");
  if (!username || username->empty())
    return default_response(false, "ユーザー名は必須です", 400);
  return result_response(
      USER_MANAGER::get_user_manager().delete_user(*username, principal->username),
      "ユーザーが正常に削除されました",
      "ユーザー削除に失敗しました");
}

crow::response promote_user(const crow::request& req) {
  const auto json = body_json(req);
  const auto principal = AUTH::principal_from_request(req);
  if (!principal)
    return default_response(false, "認証が必要です", 401);
  if (!json)
    return default_response(false, "リクエストの形式が不正です", 400);
  const auto username = string_field(*json, "username");
  if (!username || username->empty())
    return default_response(false, "ユーザー名は必須です", 400);
  return result_response(
      USER_MANAGER::get_user_manager().promote_user(*username, principal->username),
      "ユーザーが管理者に昇格されました",
      "ユーザー昇格に失敗しました");
}

crow::response demote_user(const crow::request& req) {
  const auto json = body_json(req);
  const auto principal = AUTH::principal_from_request(req);
  if (!principal)
    return default_response(false, "認証が必要です", 401);
  if (!json)
    return default_response(false, "リクエストの形式が不正です", 400);
  const auto username = string_field(*json, "username");
  if (!username || username->empty())
    return default_response(false, "ユーザー名は必須です", 400);
  return result_response(
      USER_MANAGER::get_user_manager().demote_user(*username, principal->username),
      "ユーザーが一般ユーザーに降格されました",
      "ユーザー降格に失敗しました");
}

crow::response change_password(const crow::request& req) {
  const auto principal = AUTH::principal_from_request(req);
  if (!principal)
    return default_response(false, "認証が必要です", 401);
  const auto json = body_json(req);
  if (!json)
    return default_response(false, "リクエストの形式が不正です", 400);
  const auto current = string_field(*json, "current_password");
  const auto next = string_field(*json, "new_password");
  if (!current || !next || !std::regex_match(*current, PASSWORD_RE) ||
      !std::regex_match(*next, PASSWORD_RE))
    return default_response(false, "パスワードの形式が不正です", 400);
  return result_response(
      USER_MANAGER::get_user_manager().change_password(
          principal->username, *current, *next),
      "パスワードを変更しました。再度ログインしてください",
      "パスワード変更に失敗しました");
}

crow::response get_user_list(const crow::request&) {
  const auto users = USER_MANAGER::get_user_manager().get_all_users();
  crow::json::wvalue::list user_list;
  user_list.reserve(users.size());
  for (const auto& user : users) {
    crow::json::wvalue user_data;
    user_data["username"] = user.username;
    user_data["role"] = user.role;
    user_data["created_by"] = user.created_by;
    user_data["created_at"] = user.created_at;
    user_data["last_login"] = user.last_login;
    user_list.emplace_back(std::move(user_data));
  }
  crow::json::wvalue body;
  body["success"] = true;
  body["code"] = "USER_LIST";
  body["message"] = "ユーザー一覧を取得しました";
  body["users"] = std::move(user_list);
  body["total"] = users.size();
  crow::json::wvalue data;
  crow::json::wvalue::list data_users;
  data_users.reserve(users.size());
  for (const auto& user : users) {
    crow::json::wvalue user_data;
    user_data["username"] = user.username;
    user_data["role"] = user.role;
    user_data["created_by"] = user.created_by;
    user_data["created_at"] = user.created_at;
    user_data["last_login"] = user.last_login;
    data_users.emplace_back(std::move(user_data));
  }
  data["users"] = std::move(data_users);
  data["total"] = users.size();
  body["data"] = std::move(data);
  return crow::response(200, std::move(body));
}

crow::response check_first_user(const crow::request& req) {
  const bool first = USER_MANAGER::get_user_manager().is_first_user();
  crow::json::wvalue body;
  body["success"] = true;
  body["code"] = "FIRST_USER_CHECK";
  body["is_first_user"] = first;
  body["message"] = first ? "初回ユーザーです" : "既存ユーザーが存在します";
  body["data"]["is_first_user"] = first;
  crow::response result(200, std::move(body));
  if (AUTH::extract_cookie(req, "csrf_token").empty())
    AUTH::add_csrf_cookie(result, AUTH::create_csrf_token());
  return result;
}

crow::response get_user_permissions(const crow::request& req) {
  const auto principal = AUTH::principal_from_request(req);
  if (!principal)
    return default_response(false, "認証が必要です", 401);
  crow::json::wvalue body;
  body["success"] = true;
  body["code"] = "USER_PERMISSIONS";
  body["username"] = principal->username;
  body["role"] = principal->role;
  body["is_admin"] = principal->is_admin();
  body["can_register_admin"] = principal->is_admin();
  body["can_register_user"] = principal->is_admin();
  body["can_manage_users"] = principal->is_admin();
  body["data"]["username"] = principal->username;
  body["data"]["role"] = principal->role;
  body["data"]["is_admin"] = principal->is_admin();
  body["data"]["can_register_admin"] = principal->is_admin();
  body["data"]["can_register_user"] = principal->is_admin();
  body["data"]["can_manage_users"] = principal->is_admin();
  return crow::response(200, std::move(body));
}

} // namespace USER_API
