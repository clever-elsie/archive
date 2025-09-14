#pragma once
#include "data_structure.hpp"
#include "helper.hpp"

namespace MEMO{ // 共用メモ関連のAPIエンドポイント
using namespace std;

inline crow::json::wvalue format_for_response(const SharedMemoData& memo){
  crow::json::wvalue ret;
  ret["id"] = memo.id;
  ret["title"] = memo.title;
  ret["body"] = memo.body;
  ret["created_at"] = memo.created_at;
  ret["updated_at"] = memo.updated_at;
  ret["author"] = memo.author;
  return ret;
}

// 全共用メモの取得
inline crow::response shared_memo_fetch_all(const crow::request &req) {
  vector<SharedMemoData> memos = get_all_shared_memos();
  crow::json::wvalue::list v;
  for (const auto& memo : memos)
    v.push_back(format_for_response(memo));
  return crow::response(200, crow::json::wvalue(std::move(v)));
}

// 共用メモの作成
inline crow::response shared_memo_create(const crow::request &req) {
  string token = MIDDLEWARE::extract_token(req);
  string username = AUTH::get_username_from_token(token);
  if (username.empty())
    return error_response("ユーザー情報が取得できません");
  auto data = crow::json::load(req.body);
  string title = data["title"].s();
  string body = data["body"].s();
  if (title.empty() || is_whitespace_only(title))
    return error_response("タイトルは必須です");
  string id = create_shared_memo(title, body, username);
  string timestamp = get_current_timestamp();
  SharedMemoData memo{id, title, body, timestamp, timestamp, username};
  return crow::response(200, format_for_response(memo));
}

// 共用メモの更新
inline crow::response shared_memo_update(const crow::request &req) {
  auto data = crow::json::load(req.body);
  string id = data["id"].s();
  string title = data["title"].s();
  string body = data["body"].s();
  if (id.empty()) return error_response("IDは必須です");
  if (title.empty() || is_whitespace_only(title))
    return error_response("タイトルは必須です");
  if (!update_shared_memo(id, title, body))
    return error_response("メモが見つからないか更新に失敗しました");
  return crow::response(200);
}

// 共用メモの削除
inline crow::response shared_memo_delete(const crow::request &req) {
  auto data = crow::json::load(req.body);
  string id = data["id"].s();
  if (id.empty()) return error_response("IDは必須です");
  if (!delete_shared_memo(id))
    return error_response("メモが見つからないか削除に失敗しました");
  return crow::response(200);
}

// 共用メモの取得（単体）
inline crow::response shared_memo_get(const crow::request &req) {
  auto data = crow::json::load(req.body);
  string id = data["id"].s();
  if (id.empty()) return error_response("IDは必須です");
  SharedMemoData memo = get_shared_memo(id);
  if (memo.id.empty()) return error_response("メモが見つかりません");
  return crow::response(200, format_for_response(memo));
}
} // namespace MEMO