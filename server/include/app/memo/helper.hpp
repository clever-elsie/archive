#pragma once
#include <chrono>
#include <filesystem>

#include <crow/json.h>
#include <crow/http_response.h>

#include "data_structure.hpp"

namespace MEMO {
using namespace std;

crow::json::wvalue format_for_response(const std::filesystem::path& filepath, const MemoData&memo, bool header_only);

string generate_unique_id();
string generate_unique_filename(const string& username);

bool matches_search_query( // 検索クエリを解析（AND OR NOT検索）
  const string& query,
  const string& title,
  const vector<string>& tags,
  const string& data
);

inline crow::response error_response(const std::string& message){
  crow::json::wvalue x;
  x["error"] = message;
  return crow::response(400, x);
}

// ユーザーのメモディレクトリパスを取得
inline string get_user_memo_path(const string& username) {
  return memo_base_path + username + "/";
}

// 現在のタイムスタンプを取得
inline string get_current_timestamp() {
  auto now = chrono::system_clock::now();
  auto time_t = chrono::system_clock::to_time_t(now);
  stringstream ss;
  ss << put_time(localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

// ユーザーのメモディレクトリを作成
inline bool ensure_user_directory(const string& username) {
  string user_path = get_user_memo_path(username);
  if (!filesystem::exists(user_path))
    try { return filesystem::create_directories(user_path); }
    catch (...) { return false; }
  return true;
}

// ファイル名が一意かどうかをチェック
inline bool is_filename_unique(const string& username, const string& filename) {
  const string user_path = get_user_memo_path(username);
  const string file_path = user_path + filename;
  return !filesystem::exists(file_path);
}

// データ形式が有効かどうかをチェック
inline bool is_valid_format(const string& format) {
  auto it = ranges::lower_bound(supported_formats, format);
  return it != supported_formats.end() && *it == format;
}

inline bool is_whitespace_only(const string& str) {
  return ranges::all_of(str, ::isspace);
}

}  // namespace MEMO