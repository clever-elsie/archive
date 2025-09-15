#pragma once
#include "data_structure.hpp"
#include "crow/json.h"
#include "crow/http_response.h"
#include <chrono>
#include <filesystem>

namespace MEMO {
using namespace std;

// ユーザーのメモディレクトリパスを取得
inline string get_user_memo_path(const string& username) {
  return memo_base_path + username + "/";
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

// 文字列が空白文字のみかどうかをチェック
inline bool is_whitespace_only(const string& str) {
  return ranges::all_of(str, ::isspace);
}

// 一意なファイル名を生成
inline string generate_unique_filename(const string& username) {
  // 現在のタイムスタンプをベースにしたファイル名を生成
  using namespace std::chrono;
  auto now = system_clock::now();
  auto time_t = system_clock::to_time_t(now);
  auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  stringstream ss;
  ss << put_time(localtime(&time_t), "%Y%m%d_%H%M%S");
  ss << "_" << setfill('0') << setw(3) << ms.count();
  string base_filename = ss.str() + ".json";

  // ファイル名が一意になるまで番号を追加
  string filename = base_filename;
  int counter = 1;
  while (!is_filename_unique(username, filename)) {
    string name_part = base_filename.substr(0, base_filename.find_last_of('.'));
    filename = name_part + "_" + to_string(counter) + ".json";
    counter++;
  }
  return filename;
}


// 現在のタイムスタンプを取得
inline string get_current_timestamp() {
  auto now = chrono::system_clock::now();
  auto time_t = chrono::system_clock::to_time_t(now);
  stringstream ss;
  ss << put_time(localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

// 一意なIDを生成
inline string generate_unique_id() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto time_t = system_clock::to_time_t(now);
  auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  stringstream ss;
  ss << put_time(localtime(&time_t), "%Y%m%d_%H%M%S");
  ss << "_" << setfill('0') << setw(3) << ms.count();
  return ss.str();
}

inline crow::json::wvalue format_for_response(const std::filesystem::path& filepath, const MemoData&memo, bool header_only){
  crow::json::wvalue x;
  x["filename"] = filepath.filename().string();
  x["stem"] = filepath.stem().string();
  x["extension"] = filepath.extension().string();
  x["format"] = memo.format;
  x["tag"] = crow::json::wvalue::list(memo.tag.begin(), memo.tag.end());
  x["created_at"] = memo.created_at;
  x["updated_at"] = memo.updated_at;
  if(!header_only) x["data"] = memo.data;
  return x;
}

inline crow::response error_response(const std::string& message){
  crow::json::wvalue x;
  x["error"] = message;
  return crow::response(400, x);
}

// 検索クエリを解析（AND OR NOT検索）
inline bool matches_search_query(const string& query, const string& title,
                                 const vector<string>& tags,
                                 const string& data) {
  if (query.empty()) {
    return true;  // 空のクエリはすべてにマッチ
  }

  // クエリを小文字に変換
  string lower_query = query;
  transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
            ::tolower);

  // タイトル、タグ、データを小文字に変換
  string lower_title = title;
  transform(lower_title.begin(), lower_title.end(), lower_title.begin(),
            ::tolower);

  string lower_data = data;
  transform(lower_data.begin(), lower_data.end(), lower_data.begin(),
            ::tolower);

  vector<string> lower_tags;
  for (const auto& tag : tags) {
    string lower_tag = tag;
    transform(lower_tag.begin(), lower_tag.end(), lower_tag.begin(), ::tolower);
    lower_tags.push_back(lower_tag);
  }

  // 単純なAND検索（すべての単語が含まれているかチェック）
  stringstream ss(lower_query);
  string word;
  while (ss >> word) {
    bool found = false;

    // タイトルで検索
    if (lower_title.find(word) != string::npos) {
      found = true;
    }

    // タグで検索
    for (const auto& tag : lower_tags) {
      if (tag.find(word) != string::npos) {
        found = true;
        break;
      }
    }

    // データで検索
    if (lower_data.find(word) != string::npos) {
      found = true;
    }

    if (!found) {
      return false;  // 一つの単語でも見つからない場合はマッチしない
    }
  }

  return true;
}

}  // namespace MEMO