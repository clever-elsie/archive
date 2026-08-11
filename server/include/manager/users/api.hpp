#pragma once
#include <crow/http_request.h>
#include <crow/http_response.h>

#include "manager.hpp"

namespace USER_API {
using namespace std;

crow::response register_user(const crow::request& req);
crow::response delete_user(const crow::request& req);
crow::response promote_user(const crow::request& req); // ユーザー昇格API
crow::response demote_user(const crow::request& req); // ユーザー降格API
crow::response change_password(const crow::request& req);

// ユーザー一覧取得API
crow::response get_user_list(const crow::request& req);
// 初回ユーザー確認API
crow::response check_first_user(const crow::request& req);
// ユーザー権限確認API
crow::response get_user_permissions(const crow::request& req);

} // namespace USER_API
