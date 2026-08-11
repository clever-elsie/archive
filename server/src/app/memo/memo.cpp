#include <app/memo/memo.hpp>

#include <algorithm>
#include <optional>

#include <manager/auth/auth.hpp>

namespace MEMO {

namespace {

crow::response memo_error(
    int status,
    const char* code,
    const std::string& message) {
  (void)code;
  crow::json::wvalue body;
  body["error"] = message;
  return crow::response(status, std::move(body));
}

crow::response memo_success(
    const char* code,
    const char* message,
    crow::json::wvalue data) {
  (void)code;
  (void)message;
  return crow::response(200, std::move(data));
}

crow::response memo_success(const char* code, const char* message) {
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

std::optional<std::string> string_field(
    const crow::json::rvalue& json,
    const char* name) {
  if (!json.has(name) || json[name].t() != crow::json::type::String)
    return std::nullopt;
  return json[name].s();
}

std::optional<std::filesystem::path> existing_memo_path(
    const std::string& username,
    const std::string& filename) {
  const auto path = safe_user_memo_path(username, filename);
  if (!path)
    return std::nullopt;
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(*path, ec);
  if (ec || status.type() == std::filesystem::file_type::symlink ||
      status.type() != std::filesystem::file_type::regular)
    return std::nullopt;
  return path;
}

std::optional<std::string> principal_username(const crow::request& req) {
  const auto principal = AUTH::principal_from_request(req);
  if (!principal)
    return std::nullopt;
  return principal->username;
}

bool read_tags(
    const crow::json::rvalue& json,
    std::set<std::string>& tags) {
  if (!json.has("tag"))
    return true;
  if (json["tag"].t() != crow::json::type::List)
    return false;
  for (const auto& item : json["tag"]) {
    if (item.t() != crow::json::type::String)
      return false;
    tags.insert(item.s());
  }
  return true;
}

std::vector<std::filesystem::path> memo_files(
    const std::filesystem::path& directory) {
  std::vector<std::filesystem::path> result;
  std::error_code ec;
  for (const auto& entry :
       std::filesystem::directory_iterator(
           directory,
           std::filesystem::directory_options::skip_permission_denied,
           ec)) {
    if (ec)
      break;
    const auto path = entry.path();
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec || status.type() == std::filesystem::file_type::symlink ||
        status.type() != std::filesystem::file_type::regular)
      continue;
    if (path.extension() == ".json")
      result.push_back(path);
  }
  std::ranges::sort(result);
  return result;
}

} // namespace

crow::response memo_fetch_all(const crow::request& req) {
  const auto username = principal_username(req);
  if (!username)
    return memo_error(401, "AUTH_REQUIRED", "認証が必要です");
  const auto lock = user_memo_mutex(*username);
  std::lock_guard guard(*lock);
  if (!ensure_user_directory(*username))
    return memo_error(503, "MEMO_DIRECTORY_UNAVAILABLE", "ユーザーメモ領域を利用できません");

  crow::json::wvalue::list result;
  for (const auto& path : memo_files(get_user_memo_path(*username))) {
    const MemoData memo = MemoData::load(path.string());
    if (memo.valid)
      result.emplace_back(format_for_response(path, memo, true));
  }
  return memo_success(
      "MEMO_LIST", "メモ一覧を取得しました",
      crow::json::wvalue(std::move(result)));
}

crow::response memo_search(const crow::request& req) {
  const auto username = principal_username(req);
  if (!username)
    return memo_error(401, "AUTH_REQUIRED", "認証が必要です");
  const auto lock = user_memo_mutex(*username);
  std::lock_guard guard(*lock);
	if (memo_base_path.empty())
		return memo_success(
			"MEMO_SEARCH", "メモを検索しました",
			crow::json::wvalue(crow::json::wvalue::list()));

  const char* query_value = req.url_params.get("query");
  const std::string query = query_value ? query_value : "";
  const auto query_ast = RETRIEVE::parse_query(query);
  if (!query_ast)
    return memo_error(400, "INVALID_QUERY", "検索条件が不正です");
  const auto directory = std::filesystem::path(get_user_memo_path(*username));
  std::error_code ec;
  if (!std::filesystem::is_directory(directory, ec) || ec)
    return memo_success(
        "MEMO_SEARCH", "メモを検索しました",
        crow::json::wvalue(crow::json::wvalue::list()));

  crow::json::wvalue::list result;
  for (const auto& path : memo_files(directory)) {
    const MemoData memo = MemoData::load(path.string());
    if (memo.valid && query_ast->evaluate(memo))
      result.emplace_back(format_for_response(path, memo, true));
  }
  return memo_success(
      "MEMO_SEARCH", "メモを検索しました",
      crow::json::wvalue(std::move(result)));
}

crow::response memo_create_new(const crow::request& req) {
  const auto username = principal_username(req);
  if (!username)
    return memo_error(401, "AUTH_REQUIRED", "認証が必要です");
  const auto json = body_json(req);
  if (!json)
    return memo_error(400, "BAD_REQUEST", "リクエストの形式が不正です");

  std::set<std::string> tags;
  if (!read_tags(*json, tags))
    return memo_error(400, "INVALID_TAGS", "タグの形式が不正です");
  std::string format = "txt";
  if (const auto requested = string_field(*json, "format"))
    format = *requested;
  if (!is_valid_format(format))
    return memo_error(400, "INVALID_FORMAT", "無効な形式です");

  const auto lock = user_memo_mutex(*username);
  std::lock_guard guard(*lock);
  if (!ensure_user_directory(*username))
    return memo_error(503, "MEMO_DIRECTORY_UNAVAILABLE", "ユーザーメモ領域を利用できません");
  const std::string filename = generate_unique_filename(*username);
  if (filename.empty())
    return memo_error(503, "FILENAME_UNAVAILABLE", "ファイル名を生成できません");
  const auto path = safe_user_memo_path(*username, filename);
  if (!path)
    return memo_error(400, "INVALID_FILENAME", "無効なファイル名です");

  const std::string timestamp = get_current_timestamp();
  MemoData memo{tags, std::string(), format, timestamp, timestamp, path->string()};
  if (!memo.save(path->string()))
    return memo_error(503, "MEMO_SAVE_FAILED", "メモの作成に失敗しました");
  memo.valid = true;
  return memo_success(
      "MEMO_CREATED", "メモを作成しました",
      format_for_response(*path, memo, true));
}

crow::response memo_renew(const crow::request& req) {
  const auto username = principal_username(req);
  if (!username)
    return memo_error(401, "AUTH_REQUIRED", "認証が必要です");
  const auto json = body_json(req);
  if (!json)
    return memo_error(400, "BAD_REQUEST", "リクエストの形式が不正です");
  const auto filename = string_field(*json, "filename");
  const auto data = string_field(*json, "memo");
  if (!filename || !data)
    return memo_error(400, "BAD_REQUEST", "filenameとmemoは必須です");

  const auto lock = user_memo_mutex(*username);
  std::lock_guard guard(*lock);
  const auto path = existing_memo_path(*username, *filename);
  if (!path)
    return memo_error(404, "MEMO_NOT_FOUND", "ファイルが存在しません");
  MemoData memo = MemoData::load(path->string());
  if (!memo.valid)
    return memo_error(409, "MEMO_INVALID", "メモの形式が不正です");
  memo.data = *data;
  memo.updated_at = get_current_timestamp();
  if (!memo.save(path->string()))
    return memo_error(503, "MEMO_SAVE_FAILED", "メモの保存に失敗しました");
  return memo_success("MEMO_UPDATED", "メモを保存しました");
}

crow::response memo_now(const crow::request& req) {
  const auto username = principal_username(req);
  if (!username)
    return memo_error(401, "AUTH_REQUIRED", "認証が必要です");
  const char* filename_value = req.url_params.get("filename");
  if (!filename_value)
    return memo_error(400, "FILENAME_REQUIRED", "ファイル名が指定されていません");
  const auto lock = user_memo_mutex(*username);
  std::lock_guard guard(*lock);
  const auto path = existing_memo_path(*username, filename_value);
  if (!path)
    return memo_error(404, "MEMO_NOT_FOUND", "ファイルが存在しません");
  const MemoData memo = MemoData::load(path->string());
  if (!memo.valid)
    return memo_error(409, "MEMO_INVALID", "メモの形式が不正です");
  return memo_success(
      "MEMO_FOUND", "メモを取得しました",
      format_for_response(*path, memo, false));
}

crow::response memo_rm(const crow::request& req) {
  const auto username = principal_username(req);
  if (!username)
    return memo_error(401, "AUTH_REQUIRED", "認証が必要です");
  const char* filename_value = req.url_params.get("filename");
  if (!filename_value)
    return memo_error(400, "FILENAME_REQUIRED", "ファイル名が指定されていません");
  const auto lock = user_memo_mutex(*username);
  std::lock_guard guard(*lock);
  const auto path = existing_memo_path(*username, filename_value);
  if (!path)
    return memo_error(404, "MEMO_NOT_FOUND", "ファイルが存在しません");
  std::error_code ec;
  if (!std::filesystem::remove(*path, ec) || ec)
    return memo_error(503, "MEMO_DELETE_FAILED", "ファイルの削除に失敗しました");
  return memo_success("MEMO_DELETED", "メモを削除しました");
}

crow::response memo_rename(const crow::request& req) {
  const auto username = principal_username(req);
  if (!username)
    return memo_error(401, "AUTH_REQUIRED", "認証が必要です");
  const auto json = body_json(req);
  if (!json)
    return memo_error(400, "BAD_REQUEST", "リクエストの形式が不正です");
  const auto old_filename = string_field(*json, "old_filename");
  const auto new_stem = string_field(*json, "new_stem");
  if (!old_filename || !new_stem ||
      !valid_filename_component(*new_stem))
    return memo_error(400, "INVALID_FILENAME", "無効なファイル名です");

  const auto lock = user_memo_mutex(*username);
  std::lock_guard guard(*lock);
  const auto old_path = existing_memo_path(*username, *old_filename);
  const std::string new_filename = *new_stem + ".json";
  const auto new_path = safe_user_memo_path(*username, new_filename);
  if (!old_path || !new_path)
    return memo_error(404, "MEMO_NOT_FOUND", "元のファイルが存在しません");
  std::error_code ec;
  if (std::filesystem::exists(*new_path, ec) || ec)
    return memo_error(409, "MEMO_ALREADY_EXISTS", "新しいファイル名が既に存在します");
  std::filesystem::rename(*old_path, *new_path, ec);
  if (ec)
    return memo_error(503, "MEMO_RENAME_FAILED", "メモの名前変更に失敗しました");

  crow::json::wvalue body;
  body["new_filename"] = new_filename;
  body["new_stem"] = *new_stem;
  body["extension"] = ".json";
  return memo_success(
      "MEMO_RENAMED", "メモの名前を変更しました", std::move(body));
}

crow::response memo_update_tags(const crow::request& req) {
  const auto username = principal_username(req);
  if (!username)
    return memo_error(401, "AUTH_REQUIRED", "認証が必要です");
  const auto json = body_json(req);
  if (!json)
    return memo_error(400, "BAD_REQUEST", "リクエストの形式が不正です");
  const auto filename = string_field(*json, "filename");
  if (!filename)
    return memo_error(400, "FILENAME_REQUIRED", "ファイル名が指定されていません");
  std::set<std::string> tags;
  if (!read_tags(*json, tags))
    return memo_error(400, "INVALID_TAGS", "タグの形式が不正です");

  const auto lock = user_memo_mutex(*username);
  std::lock_guard guard(*lock);
  const auto path = existing_memo_path(*username, *filename);
  if (!path)
    return memo_error(404, "MEMO_NOT_FOUND", "ファイルが存在しません");
  MemoData memo = MemoData::load(path->string());
  if (!memo.valid)
    return memo_error(409, "MEMO_INVALID", "メモの形式が不正です");
  memo.tag = std::move(tags);
  memo.updated_at = get_current_timestamp();
  if (!memo.save(path->string()))
    return memo_error(503, "MEMO_SAVE_FAILED", "タグの更新に失敗しました");
  return memo_success("MEMO_TAGS_UPDATED", "タグを更新しました");
}

crow::response memo_get_formats(const crow::request&) {
  crow::json::wvalue::list formats;
  for (const auto format : supported_formats)
    formats.emplace_back(std::string(format));
  crow::json::wvalue body;
  body["formats"] = std::move(formats);
  return memo_success(
      "MEMO_FORMATS", "利用可能な形式を取得しました", std::move(body));
}

crow::response memo_check_title(const crow::request& req) {
  const auto username = principal_username(req);
  if (!username)
    return memo_error(401, "AUTH_REQUIRED", "認証が必要です");
  const char* title_value = req.url_params.get("title");
  if (!title_value || !valid_filename_component(title_value) ||
      is_whitespace_only(title_value))
    return memo_error(400, "INVALID_FILENAME", "タイトルは必須です");
  const std::string filename = std::string(title_value) + ".json";
  const auto lock = user_memo_mutex(*username);
  std::lock_guard guard(*lock);
  if (!ensure_user_directory(*username))
    return memo_error(503, "MEMO_DIRECTORY_UNAVAILABLE", "ユーザーメモ領域を利用できません");
  const bool available = is_filename_unique(*username, filename);
  crow::json::wvalue body;
  body["available"] = available;
  body["title"] = title_value;
  if (!available)
    body["error"] = "このタイトルは既に使用されています";
  return memo_success(
      "MEMO_TITLE_CHECK", "タイトルを確認しました", std::move(body));
}

crow::response memo_create_with_title(const crow::request& req) {
  const auto username = principal_username(req);
  if (!username)
    return memo_error(401, "AUTH_REQUIRED", "認証が必要です");
  const auto json = body_json(req);
  if (!json)
    return memo_error(400, "BAD_REQUEST", "リクエストの形式が不正です");
  const auto title = string_field(*json, "title");
  if (!title || !valid_filename_component(*title) ||
      is_whitespace_only(*title))
    return memo_error(400, "INVALID_FILENAME", "タイトルは必須です");
  std::set<std::string> tags;
  if (!read_tags(*json, tags))
    return memo_error(400, "INVALID_TAGS", "タグの形式が不正です");
  std::string format = "txt";
  if (const auto requested = string_field(*json, "format"))
    format = *requested;
  if (!is_valid_format(format))
    return memo_error(400, "INVALID_FORMAT", "無効な形式です");

  const auto lock = user_memo_mutex(*username);
  std::lock_guard guard(*lock);
  if (!ensure_user_directory(*username))
    return memo_error(503, "MEMO_DIRECTORY_UNAVAILABLE", "ユーザーメモ領域を利用できません");
  const std::string filename = *title + ".json";
  if (!is_filename_unique(*username, filename))
    return memo_error(409, "MEMO_ALREADY_EXISTS", "このタイトルは既に使用されています");
  const auto path = safe_user_memo_path(*username, filename);
  if (!path)
    return memo_error(400, "INVALID_FILENAME", "無効なファイル名です");
  const std::string timestamp = get_current_timestamp();
  MemoData memo{tags, std::string(), format, timestamp, timestamp, path->string()};
  if (!memo.save(path->string()))
    return memo_error(503, "MEMO_SAVE_FAILED", "メモの作成に失敗しました");
  memo.valid = true;
  return memo_success(
      "MEMO_CREATED", "メモを作成しました",
      format_for_response(*path, memo, true));
}

} // namespace MEMO
