#pragma once
#include <cctype>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>

#include <crow/json.h>
#include <crow/http_response.h>

#include "data_structure.hpp"

namespace MEMO {
using namespace std;

crow::json::wvalue format_for_response(const std::filesystem::path& filepath, const MemoData&memo, bool header_only);

string generate_unique_id();
string generate_unique_filename(const string& username);
bool valid_filename_component(const string& filename);
optional<filesystem::path> safe_user_memo_path(
  const string& username,
  const string& filename);

bool matches_search_query( // 検索クエリを解析（AND OR NOT検索）
  const string& query,
  const string& title,
  const vector<string>& tags,
  const string& data
);

// ユーザーのメモディレクトリパスを取得
string get_user_memo_path(const string& username);

// 現在のタイムスタンプを取得
string get_current_timestamp();

// ユーザーのメモディレクトリを作成
bool ensure_user_directory(const string& username);

// ファイル名が一意かどうかをチェック
bool is_filename_unique(const string& username, const string& filename);

// データ形式が有効かどうかをチェック
inline bool is_valid_format(const string& format) {
  auto it = ranges::lower_bound(supported_formats, format);
  return it != supported_formats.end() && *it == format;
}

inline bool is_whitespace_only(const string& str) {
  return ranges::all_of(str, [](unsigned char value) {
    return std::isspace(value) != 0;
  });
}

}  // namespace MEMO
