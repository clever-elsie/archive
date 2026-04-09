#include <app/memo/memo.hpp>
#include <manager/auth/middleware.hpp>

namespace MEMO{

crow::response memo_fetch_all(const crow::request &req) {
  string token = MIDDLEWARE::extract_token(req);
  string username = AUTH::get_username_from_token(token);
  if (username.empty())
    return error_response("ユーザー情報が取得できません");
  
  lock_guard<mutex> lock(mmtex);
  
  // ユーザーディレクトリを確保
  if (!ensure_user_directory(username))
    return error_response("ユーザーディレクトリの作成に失敗しました");
  
  string user_path = get_user_memo_path(username);
  crow::json::wvalue::list v;
  
  if (filesystem::exists(user_path)) {
    for (const auto &file : filesystem::directory_iterator(user_path)) {
      filesystem::path path = file.path();
      if (path.extension().string() == ".json") {
        MemoData memo = MemoData::load(path.string());
        v.push_back(format_for_response(path.string(), memo, true));
      }
    }
  }
  
  return crow::response(200, crow::json::wvalue(std::move(v)));
}

crow::response memo_search(const crow::request &req) {
  string token = MIDDLEWARE::extract_token(req);
  string username = AUTH::get_username_from_token(token);
  if (username.empty())
    return error_response("ユーザー情報が取得できません");
  
  lock_guard<mutex> lock(mmtex);
  
  const char* query_c = req.url_params.get("query");
  string query = query_c ? string(query_c) : string();
  
  string user_path = get_user_memo_path(username);
  crow::json::wvalue::list v;
  if(!filesystem::exists(user_path))
    return crow::response(200, crow::json::wvalue(std::move(v)));
  const auto queryAST = RETRIEVE::parse_query(query);
  if(!queryAST) return crow::response(400);
  for (const auto &file : filesystem::directory_iterator(user_path)) {
    filesystem::path path = file.path();
    if (path.extension().string() == ".json") {
      MemoData memo = MemoData::load(path.string());
      if (queryAST->evaluate(memo))
        v.push_back(format_for_response(path.string(), memo, true));
    }
  }
  return crow::response(200, crow::json::wvalue(std::move(v)));
}

crow::response memo_create_new(const crow::request &req) {
  string token = MIDDLEWARE::extract_token(req);
  string username = AUTH::get_username_from_token(token);
  if (username.empty())
    return error_response("ユーザー情報が取得できません");
  
  lock_guard<mutex> lock(mmtex);
  
  auto data = crow::json::load(req.body);
  set<string> tag;
  string format = "txt"; // デフォルトはtxt
  
  // タグを読み込み
  if (data.has("tag")) {
    auto tag_array = data["tag"];
    for (size_t i = 0; i < tag_array.size(); i++)
      tag.insert(tag_array[i].s());
  }
  
  // 形式を読み込み
  if (data.has("format")) {
    format = data["format"].s();
    if (!is_valid_format(format))
      return error_response("無効な形式です");
  }
  
  // ユーザーディレクトリを確保
  if (!ensure_user_directory(username)) 
    return error_response("ユーザーディレクトリの作成に失敗しました");
  
  // 一意なファイル名を生成
  string filename = generate_unique_filename(username);
  if (filename.empty())
    return error_response("ファイル名の生成に失敗しました");
  
  const string file_path = get_user_memo_path(username) + filename;
  const string timestamp = get_current_timestamp();
  MemoData memo{tag, "", format, timestamp, timestamp, file_path};
  if (!memo.save(file_path))
    return error_response("メモの作成に失敗しました");
  return crow::response(200, format_for_response(file_path, memo, true));
}

crow::response memo_renew(const crow::request &req) {
  string token = MIDDLEWARE::extract_token(req);
  string username = AUTH::get_username_from_token(token);
  if (username.empty())
    return error_response("ユーザー情報が取得できません");
  lock_guard<mutex> lock(mmtex);
  auto data = crow::json::load(req.body);
  string filename = data["filename"].s();
  string new_data = data["memo"].s();
  // ファイル名の安全性をチェック
  for(const auto& pattern : {"..", "/", "\\"})
    if (filename.find(pattern) != string::npos)
      return error_response("無効なファイル名です");
  
  string file_path = get_user_memo_path(username) + filename;
  if (!filesystem::exists(file_path))
    return error_response("ファイルが存在しません");

  MemoData memo = MemoData::load(file_path);
  memo.data = new_data;
  memo.updated_at = get_current_timestamp();
  if (!memo.save(file_path))
    return error_response("メモの保存に失敗しました");
  return crow::response(200);
}

crow::response memo_now(const crow::request& req) {
  string token = MIDDLEWARE::extract_token(req);
  string username = AUTH::get_username_from_token(token);
  if (username.empty())
    return error_response("ユーザー情報が取得できません");
  
  lock_guard<mutex> lock(mmtex);
  
  const char* fn_c = req.url_params.get("filename");
  if(!fn_c) return error_response("ファイル名が指定されていません");
  string filename(fn_c);
  
  for(const auto& pattern : {"..", "/", "\\"})
    if (filename.find(pattern) != string::npos)
      return error_response("無効なファイル名です");
  
  string file_path = get_user_memo_path(username) + filename;
  
  if (!filesystem::exists(file_path))
    return error_response("ファイルが存在しません");
  
  MemoData memo = MemoData::load(file_path);
  return crow::response(200, format_for_response(file_path, memo, false));
}

crow::response memo_rm(const crow::request &req) {
  string token = MIDDLEWARE::extract_token(req);
  string username = AUTH::get_username_from_token(token);
  if (username.empty())
    return error_response("ユーザー情報が取得できません");
  lock_guard<mutex> lock(mmtex);
  const char* fn_c = req.url_params.get("filename");
  if(!fn_c) return error_response("ファイル名が指定されていません");
  string filename(fn_c);
  for(const auto& pattern : {"..", "/", "\\"})
    if (filename.find(pattern) != string::npos)
      return error_response("無効なファイル名です");
  string file_path = get_user_memo_path(username) + filename;
  if (!filesystem::exists(file_path))
    return error_response("ファイルが存在しません");
  if (!filesystem::remove(file_path))
    return error_response("ファイルの削除に失敗しました");
  return crow::response(200);
}

crow::response memo_rename(const crow::request &req) {
  string token = MIDDLEWARE::extract_token(req);
  string username = AUTH::get_username_from_token(token);
  if (username.empty())
    return error_response("ユーザー情報が取得できません");
  
  lock_guard<mutex> lock(mmtex);
  
  auto data = crow::json::load(req.body);
  string old_filename = data["old_filename"].s();
  string new_stem = data["new_stem"].s();
  
  // ファイル名の安全性をチェック
  for(const auto& pattern : {"..", "/", "\\"})
    if (old_filename.find(pattern) != string::npos || new_stem.find(pattern) != string::npos)
      return error_response("無効なファイル名です");
  
  
  // 新しいファイル名を作成
  string new_filename = new_stem + ".json";
  
  string old_path = get_user_memo_path(username) + old_filename;
  string new_path = get_user_memo_path(username) + new_filename;
  
  if (!filesystem::exists(old_path))
    return error_response("元のファイルが存在しません");
  
  if (filesystem::exists(new_path))
    return error_response("新しいファイル名が既に存在します");
  
  MemoData memo = MemoData::load(old_path);
  memo.path = new_path;
  memo.updated_at = get_current_timestamp();
  if (!memo.save(new_path))
    return error_response("メモの保存に失敗しました");
  try{
    filesystem::remove(old_path);
  }catch(...){
    return error_response("元のファイルの削除に失敗しました");
  }
  
  // 成功レスポンスに新しいファイル名情報を含める
  crow::json::wvalue ret;
  ret["new_filename"] = new_filename;
  ret["new_stem"] = new_stem;
  ret["extension"] = ".json";
  return crow::response(200, std::move(ret));
}

crow::response memo_update_tags(const crow::request &req) {
  string token = MIDDLEWARE::extract_token(req);
  string username = AUTH::get_username_from_token(token);
  if (username.empty())
    return error_response("ユーザー情報が取得できません");
  
  lock_guard<mutex> lock(mmtex);
  auto data = crow::json::load(req.body);
  string filename = data["filename"].s();
  set<string> new_tag;
  for(const auto& pattern : {"..", "/", "\\"})
    if (filename.find(pattern) != string::npos)
      return error_response("無効なファイル名です");
  if (data.has("tag"))
    for(const auto&x:data["tag"])
      new_tag.insert(x.s());
  string file_path = get_user_memo_path(username) + filename;
  if (!filesystem::exists(file_path))
    return error_response("ファイルが存在しません");
  MemoData memo = MemoData::load(file_path);
  memo.tag = std::move(new_tag);
  memo.updated_at = get_current_timestamp();
  if (!memo.save(file_path))
    return error_response("タグの更新に失敗しました");
  return crow::response(200);
}

crow::response memo_get_formats(const crow::request &req) {
  crow::json::wvalue::list formats_list;
  for (const auto& format : supported_formats)
    formats_list.push_back(std::string(format));
  
  crow::json::wvalue ret;
  ret["formats"] = std::move(formats_list);
  return crow::response(200, std::move(ret));
}

crow::response memo_check_title(const crow::request &req) {
  string token = MIDDLEWARE::extract_token(req);
  string username = AUTH::get_username_from_token(token);
  if (username.empty())
    return error_response("ユーザー情報が取得できません");
  
  lock_guard<mutex> lock(mmtex);
  
  const char* title_c = req.url_params.get("title");
  if(!title_c) return error_response("タイトルは必須です");
  string title(title_c);
  
  // タイトルの安全性をチェック
  for(const auto& pattern : {"..", "/", "\\"})
    if (title.find(pattern) != string::npos)
      return error_response("無効なタイトルです");
  
  // タイトルが空でないことをチェック
  if (title.empty() || is_whitespace_only(title))
    return error_response("タイトルは必須です");
  
  // ファイル名として使用可能かチェック
  string filename = title + ".json";
  bool is_available = is_filename_unique(username, filename);
  
  crow::json::wvalue ret;
  ret["available"] = is_available;
  ret["title"] = title;
  if (!is_available)
    ret["error"] = "このタイトルは既に使用されています";
  return crow::response(200, std::move(ret));
}

crow::response memo_create_with_title(const crow::request &req) {
  string token = MIDDLEWARE::extract_token(req);
  string username = AUTH::get_username_from_token(token);
  if (username.empty())
    return error_response("ユーザー情報が取得できません");
  
  lock_guard<mutex> lock(mmtex);
  
  auto data = crow::json::load(req.body);
  string title = data["title"].s();
  set<string> tag;
  string format = "txt"; // デフォルトはtxt
  
  // タイトルの安全性をチェック
  for(const auto& pattern : {"..", "/", "\\"})
    if (title.find(pattern) != string::npos)
      return error_response("無効なタイトルです");
  if (title.empty() || is_whitespace_only(title))
    return error_response("タイトルは必須です");
  
  // タグを読み込み
  if (data.has("tag"))
    for(const auto&x:data["tag"])
      tag.insert(x.s());
  
  // 形式を読み込み
  if (data.has("format")) {
    format = data["format"].s();
    if (!is_valid_format(format))
      return error_response("無効な形式です");
  }
  
  // ユーザーディレクトリを確保
  if (!ensure_user_directory(username))
    return error_response("ユーザーディレクトリの作成に失敗しました");
  
  // ファイル名を作成
  string filename = title + ".json";
  
  // ファイル名の一意性をチェック
  if (!is_filename_unique(username, filename))
    return error_response("このタイトルは既に使用されています");
  
  string file_path = get_user_memo_path(username) + filename;
  string timestamp = get_current_timestamp();
  // 新しいメモデータを作成
  MemoData memo{tag, "", format, timestamp, timestamp, file_path};
  // JSONファイルとして保存
  if (!memo.save(file_path))
    return error_response("メモの作成に失敗しました");
  
  return crow::response(200, format_for_response(file_path, memo, true));
}
} // namespace MEMO