#include <app/memo/shared_memo.hpp>

#include <optional>

#include <manager/auth/auth.hpp>

namespace MEMO {

namespace {

crow::response shared_error(
    int status,
    const char* code,
    const std::string& message) {
  (void)code;
  crow::json::wvalue body;
  body["error"] = message;
  return crow::response(status, std::move(body));
}

crow::response shared_success(
    const char* code,
    const char* message,
    crow::json::wvalue data) {
  (void)code;
  (void)message;
  return crow::response(200, std::move(data));
}

crow::response shared_success(const char* code, const char* message) {
  (void)code;
  (void)message;
  return crow::response(200);
}

std::optional<crow::json::rvalue> body_json(const crow::request& req) {
  auto json = crow::json::load(req.body);
  if (!json)
    return std::nullopt;
  // rvalue owns the parser buffer through its move-only ownership state.
  // Copying it into optional leaves fields pointing at freed memory.
  return std::move(json);
}

std::optional<std::string> field(
    const crow::json::rvalue& json,
    const char* name) {
  if (!json.has(name) || json[name].t() != crow::json::type::String)
    return std::nullopt;
  return json[name].s();
}

std::optional<AUTH::Principal> principal(const crow::request& req) {
  return AUTH::principal_from_request(req);
}

} // namespace

crow::json::wvalue format_for_response(
    const SharedMemoData& memo,
    bool can_edit) {
  crow::json::wvalue result;
  result["id"] = memo.id;
  result["title"] = memo.title;
  result["body"] = memo.body;
  result["created_at"] = memo.created_at;
  result["updated_at"] = memo.updated_at;
  result["author"] = memo.author;
  result["can_edit"] = can_edit;
  return result;
}

crow::response shared_memo_fetch_all(const crow::request& req) {
  const auto actor = principal(req);
  if (!actor)
    return shared_error(401, "AUTH_REQUIRED", "認証が必要です");
  crow::json::wvalue::list result;
  for (const auto& memo : get_all_shared_memos())
    result.emplace_back(format_for_response(
        memo, !memo.author_is_admin || actor->is_admin()));
  return shared_success(
      "SHARED_MEMO_LIST", "共用メモ一覧を取得しました",
      crow::json::wvalue(std::move(result)));
}

crow::response shared_memo_create(const crow::request& req) {
  const auto actor = principal(req);
  if (!actor)
    return shared_error(401, "AUTH_REQUIRED", "認証が必要です");
  const auto json = body_json(req);
  if (!json)
    return shared_error(400, "BAD_REQUEST", "リクエストの形式が不正です");
  const auto title = field(*json, "title");
  const auto body = field(*json, "body");
  if (!title || !body || title->empty() || is_whitespace_only(*title))
    return shared_error(400, "INVALID_TITLE", "タイトルは必須です");
  const std::string id = create_shared_memo(
      *title, *body, actor->username, actor->is_admin());
  const auto memo = get_shared_memo(id);
  return shared_success(
      "SHARED_MEMO_CREATED", "共用メモを作成しました",
      format_for_response(memo, true));
}

crow::response shared_memo_update(const crow::request& req) {
  const auto actor = principal(req);
  if (!actor)
    return shared_error(401, "AUTH_REQUIRED", "認証が必要です");
  const auto json = body_json(req);
  if (!json)
    return shared_error(400, "BAD_REQUEST", "リクエストの形式が不正です");
  const auto id = field(*json, "id");
  const auto title = field(*json, "title");
  const auto body = field(*json, "body");
  if (!id || !title || !body || id->empty() || title->empty() ||
      is_whitespace_only(*title))
    return shared_error(400, "BAD_REQUEST", "id、title、bodyは必須です");
  if (!update_shared_memo(*id, *title, *body, actor->is_admin()))
    return shared_error(404, "SHARED_MEMO_NOT_FOUND", "メモが見つからないか権限がありません");
  return shared_success("SHARED_MEMO_UPDATED", "共用メモを更新しました");
}

crow::response shared_memo_delete(const crow::request& req) {
  const auto actor = principal(req);
  if (!actor)
    return shared_error(401, "AUTH_REQUIRED", "認証が必要です");
  const char* id_value = req.url_params.get("id");
  if (!id_value || std::string(id_value).empty())
    return shared_error(400, "ID_REQUIRED", "IDは必須です");
  if (!delete_shared_memo(id_value, actor->is_admin()))
    return shared_error(404, "SHARED_MEMO_NOT_FOUND", "メモが見つからないか権限がありません");
  return shared_success("SHARED_MEMO_DELETED", "共用メモを削除しました");
}

crow::response shared_memo_get(const crow::request& req) {
  const auto actor = principal(req);
  if (!actor)
    return shared_error(401, "AUTH_REQUIRED", "認証が必要です");
  const char* id_value = req.url_params.get("id");
  if (!id_value || std::string(id_value).empty())
    return shared_error(400, "ID_REQUIRED", "IDは必須です");
  const auto memo = get_shared_memo(id_value);
  if (memo.id.empty())
    return shared_error(404, "SHARED_MEMO_NOT_FOUND", "メモが見つかりません");
  return shared_success(
      "SHARED_MEMO_FOUND", "共用メモを取得しました",
      format_for_response(memo, !memo.author_is_admin || actor->is_admin()));
}

} // namespace MEMO
