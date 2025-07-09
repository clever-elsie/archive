#pragma once
#include "../../headers.hpp"
#include "../../manager/auth/auth.hpp"
#include <algorithm>
#include <cctype>

namespace MEMO{
using namespace std;
using i64=int64_t;
using u64=uint64_t;

// グローバル変数
inline string memo_base_path;
inline mutex mmtex;

// サポートされているデータ形式
inline vector<string> supported_formats = {"md", "txt", "json"};

// メモのJSON構造
struct MemoData {
    vector<string> tags;
    string data;
    string format;  // "md", "txt", "json"のいずれか
    string created_at;
    string updated_at;
};

// ユーザーのメモディレクトリパスを取得
inline string get_user_memo_path(const string& username) {
    return memo_base_path + username + "/";
}

// ユーザーのメモディレクトリを作成
inline bool ensure_user_directory(const string& username) {
    string user_path = get_user_memo_path(username);
    if (!filesystem::exists(user_path)) {
        try {
            return filesystem::create_directories(user_path);
        } catch (...) {
            return false;
        }
    }
    return true;
}

// ファイル名が一意かどうかをチェック
inline bool is_filename_unique(const string& username, const string& filename) {
    string user_path = get_user_memo_path(username);
    string file_path = user_path + filename;
    return !filesystem::exists(file_path);
}

// データ形式が有効かどうかをチェック
inline bool is_valid_format(const string& format) {
    return find(supported_formats.begin(), supported_formats.end(), format) != supported_formats.end();
}

// 文字列が空白文字のみかどうかをチェック
inline bool is_whitespace_only(const string& str) {
    return all_of(str.begin(), str.end(), ::isspace);
}

// 一意なファイル名を生成
inline string generate_unique_filename(const string& username) {
    // 現在のタイムスタンプをベースにしたファイル名を生成
    auto now = chrono::system_clock::now();
    auto time_t = chrono::system_clock::to_time_t(now);
    auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
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

// JSONからMemoDataを読み込み
inline MemoData load_memo_data(const string& file_path) {
    MemoData memo;
    ifstream ifs(file_path);
    if (ifs) {
        string json_str{istreambuf_iterator<char>(ifs), istreambuf_iterator<char>()};
        auto json_data = crow::json::load(json_str);
        if (json_data) {
            // タグを読み込み
            if (json_data.has("tags")) {
                auto tags_array = json_data["tags"];
                for (size_t i = 0; i < tags_array.size(); i++) {
                    memo.tags.push_back(tags_array[i].s());
                }
            }
            // データを読み込み
            if (json_data.has("data")) {
                memo.data = json_data["data"].s();
            }
            // 形式を読み込み
            if (json_data.has("format")) {
                memo.format = json_data["format"].s();
            } else {
                memo.format = "txt"; // デフォルトはtxt
            }
            // 作成日時を読み込み
            if (json_data.has("created_at")) {
                memo.created_at = json_data["created_at"].s();
            }
            // 更新日時を読み込み
            if (json_data.has("updated_at")) {
                memo.updated_at = json_data["updated_at"].s();
            }
        }
    }
    return memo;
}

// MemoDataをJSONとして保存
inline bool save_memo_data(const string& file_path, const MemoData& memo) {
    crow::json::wvalue json_data;
    
    // タグを保存
    crow::json::wvalue::list tags_list;
    for (const auto& tag : memo.tags) {
        tags_list.push_back(tag);
    }
    json_data["tags"] = std::move(tags_list);
    
    // データを保存
    json_data["data"] = memo.data;
    
    // 形式を保存
    json_data["format"] = memo.format;
    
    // 日時を保存
    json_data["created_at"] = memo.created_at;
    json_data["updated_at"] = memo.updated_at;
    
    ofstream ofs(file_path);
    if (!ofs) {
        return false;
    }
    
    ofs << json_data.dump();
    return true;
}

// 現在のタイムスタンプを取得
inline string get_current_timestamp() {
    auto now = chrono::system_clock::now();
    auto time_t = chrono::system_clock::to_time_t(now);
    stringstream ss;
    ss << put_time(localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// 検索クエリを解析（AND OR NOT検索）
inline bool matches_search_query(const string& query, const string& title, const vector<string>& tags, const string& data) {
    if (query.empty()) {
        return true; // 空のクエリはすべてにマッチ
    }
    
    // クエリを小文字に変換
    string lower_query = query;
    transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    
    // タイトル、タグ、データを小文字に変換
    string lower_title = title;
    transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);
    
    string lower_data = data;
    transform(lower_data.begin(), lower_data.end(), lower_data.begin(), ::tolower);
    
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
            return false; // 一つの単語でも見つからない場合はマッチしない
        }
    }
    
    return true;
}

inline crow::response memo_fetch_all(const crow::request &req) {
    string session_id = MIDDLEWARE::extract_session_id(req);
    if (session_id.empty() || !AUTH::validate_session(session_id)) {
        crow::json::wvalue error_response;
        error_response["error"] = "認証が必要です";
        return crow::response(401, error_response);
    }
    
    string username = AUTH::get_username_from_session(session_id);
    if (username.empty()) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザー情報が取得できません";
        return crow::response(400, error_response);
    }
    
    lock_guard<mutex> lock(mmtex);
    
    // ユーザーディレクトリを確保
    if (!ensure_user_directory(username)) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザーディレクトリの作成に失敗しました";
        return crow::response(500, error_response);
    }
    
    string user_path = get_user_memo_path(username);
    crow::json::wvalue::list v;
    
    if (filesystem::exists(user_path)) {
        for (const auto &file : filesystem::directory_iterator(user_path)) {
            filesystem::path path = file.path();
            if (path.extension().string() == ".json") {
                string filename = path.filename().string();
                string stem = filename.substr(0, filename.find_last_of('.'));
                
                // メモデータを読み込み
                MemoData memo = load_memo_data(path.string());
                
                crow::json::wvalue x;
                x["filename"] = filename;
                x["stem"] = stem;
                x["extension"] = ".json";
                x["format"] = memo.format;
                x["tags"] = memo.tags;
                x["created_at"] = memo.created_at;
                x["updated_at"] = memo.updated_at;
                v.push_back(std::move(x));
            }
        }
    }
    
    return crow::response(200, crow::json::wvalue(std::move(v)));
}

inline crow::response memo_search(const crow::request &req) {
    string session_id = MIDDLEWARE::extract_session_id(req);
    if (session_id.empty() || !AUTH::validate_session(session_id)) {
        crow::json::wvalue error_response;
        error_response["error"] = "認証が必要です";
        return crow::response(401, error_response);
    }
    
    string username = AUTH::get_username_from_session(session_id);
    if (username.empty()) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザー情報が取得できません";
        return crow::response(400, error_response);
    }
    
    lock_guard<mutex> lock(mmtex);
    
    auto data = crow::json::load(req.body);
    string query = data["query"].s();
    
    string user_path = get_user_memo_path(username);
    crow::json::wvalue::list v;
    
    if (filesystem::exists(user_path)) {
        for (const auto &file : filesystem::directory_iterator(user_path)) {
            filesystem::path path = file.path();
            if (path.extension().string() == ".json") {
                string filename = path.filename().string();
                string stem = filename.substr(0, filename.find_last_of('.'));
                
                // メモデータを読み込み
                MemoData memo = load_memo_data(path.string());
                
                // 検索クエリにマッチするかチェック
                if (matches_search_query(query, stem, memo.tags, memo.data)) {
                    crow::json::wvalue x;
                    x["filename"] = filename;
                    x["stem"] = stem;
                    x["extension"] = ".json";
                    x["format"] = memo.format;
                    x["tags"] = memo.tags;
                    x["created_at"] = memo.created_at;
                    x["updated_at"] = memo.updated_at;
                    v.push_back(std::move(x));
                }
            }
        }
    }
    
    return crow::response(200, crow::json::wvalue(std::move(v)));
}

inline crow::response memo_create_new(const crow::request &req) {
    string session_id = MIDDLEWARE::extract_session_id(req);
    if (session_id.empty() || !AUTH::validate_session(session_id)) {
        crow::json::wvalue error_response;
        error_response["error"] = "認証が必要です";
        return crow::response(401, error_response);
    }
    
    string username = AUTH::get_username_from_session(session_id);
    if (username.empty()) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザー情報が取得できません";
        return crow::response(400, error_response);
    }
    
    lock_guard<mutex> lock(mmtex);
    
    auto data = crow::json::load(req.body);
    vector<string> tags;
    string format = "txt"; // デフォルトはtxt
    
    // タグを読み込み
    if (data.has("tags")) {
        auto tags_array = data["tags"];
        for (size_t i = 0; i < tags_array.size(); i++) {
            tags.push_back(tags_array[i].s());
        }
    }
    
    // 形式を読み込み
    if (data.has("format")) {
        format = data["format"].s();
        if (!is_valid_format(format)) {
            crow::json::wvalue error_response;
            error_response["error"] = "無効な形式です";
            return crow::response(400, error_response);
        }
    }
    
    // ユーザーディレクトリを確保
    if (!ensure_user_directory(username)) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザーディレクトリの作成に失敗しました";
        return crow::response(500, error_response);
    }
    
    // 一意なファイル名を生成
    string filename = generate_unique_filename(username);
    if (filename.empty()) {
        crow::json::wvalue error_response;
        error_response["error"] = "ファイル名の生成に失敗しました";
        return crow::response(500, error_response);
    }
    
    string file_path = get_user_memo_path(username) + filename;
    string stem = filename.substr(0, filename.find_last_of('.'));
    string timestamp = get_current_timestamp();
    
    // 新しいメモデータを作成
    MemoData memo;
    memo.tags = tags;
    memo.data = "";
    memo.format = format;
    memo.created_at = timestamp;
    memo.updated_at = timestamp;
    
    // JSONファイルとして保存
    if (!save_memo_data(file_path, memo)) {
        crow::json::wvalue error_response;
        error_response["error"] = "メモの作成に失敗しました";
        return crow::response(500, error_response);
    }
    
    crow::json::wvalue ret;
    ret["filename"] = filename;
    ret["stem"] = stem;
    ret["extension"] = ".json";
    ret["format"] = format;
    ret["tags"] = tags;
    ret["data"] = "";
    ret["created_at"] = timestamp;
    ret["updated_at"] = timestamp;
    return crow::response(200, std::move(ret));
}

inline crow::response memo_renew(const crow::request &req) {
    string session_id = MIDDLEWARE::extract_session_id(req);
    if (session_id.empty() || !AUTH::validate_session(session_id)) {
        crow::json::wvalue error_response;
        error_response["error"] = "認証が必要です";
        return crow::response(401, error_response);
    }
    
    string username = AUTH::get_username_from_session(session_id);
    if (username.empty()) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザー情報が取得できません";
        return crow::response(400, error_response);
    }
    
    lock_guard<mutex> lock(mmtex);
    
    auto data = crow::json::load(req.body);
    string filename = data["filename"].s();
    string new_data = data["memo"].s();
    
    // ファイル名の安全性をチェック
    if (filename.find("..") != string::npos || filename.find("/") != string::npos || filename.find("\\") != string::npos) {
        crow::json::wvalue error_response;
        error_response["error"] = "無効なファイル名です";
        return crow::response(400, error_response);
    }
    
    string file_path = get_user_memo_path(username) + filename;
    
    // ファイルが存在するかチェック
    if (!filesystem::exists(file_path)) {
        crow::json::wvalue error_response;
        error_response["error"] = "ファイルが存在しません";
        return crow::response(404, error_response);
    }
    
    // 既存のメモデータを読み込み
    MemoData memo = load_memo_data(file_path);
    
    // データを更新
    memo.data = new_data;
    memo.updated_at = get_current_timestamp();
    
    // 保存
    if (!save_memo_data(file_path, memo)) {
        crow::json::wvalue error_response;
        error_response["error"] = "メモの保存に失敗しました";
        return crow::response(500, error_response);
    }
    
    return crow::response(200);
}

inline crow::response memo_now(const crow::request& req) {
    string session_id = MIDDLEWARE::extract_session_id(req);
    if (session_id.empty() || !AUTH::validate_session(session_id)) {
        crow::json::wvalue error_response;
        error_response["error"] = "認証が必要です";
        return crow::response(401, error_response);
    }
    
    string username = AUTH::get_username_from_session(session_id);
    if (username.empty()) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザー情報が取得できません";
        return crow::response(400, error_response);
    }
    
    lock_guard<mutex> lock(mmtex);
    
    auto data = crow::json::load(req.body);
    string filename = data["filename"].s();
    
    // ファイル名の安全性をチェック
    if (filename.find("..") != string::npos || filename.find("/") != string::npos || filename.find("\\") != string::npos) {
        crow::json::wvalue error_response;
        error_response["error"] = "無効なファイル名です";
        return crow::response(400, error_response);
    }
    
    string file_path = get_user_memo_path(username) + filename;
    
    if (!filesystem::exists(file_path)) {
        crow::json::wvalue error_response;
        error_response["error"] = "ファイルが存在しません";
        return crow::response(404, error_response);
    }
    
    // メモデータを読み込み
    MemoData memo = load_memo_data(file_path);
    string stem = filename.substr(0, filename.find_last_of('.'));
    
    crow::json::wvalue ret;
    ret["filename"] = filename;
    ret["stem"] = stem;
    ret["extension"] = ".json";
    ret["format"] = memo.format;
    ret["tags"] = memo.tags;
    ret["data"] = memo.data;
    ret["created_at"] = memo.created_at;
    ret["updated_at"] = memo.updated_at;
    return crow::response(ret);
}

inline crow::response memo_rm(const crow::request &req) {
    string session_id = MIDDLEWARE::extract_session_id(req);
    if (session_id.empty() || !AUTH::validate_session(session_id)) {
        crow::json::wvalue error_response;
        error_response["error"] = "認証が必要です";
        return crow::response(401, error_response);
    }
    
    string username = AUTH::get_username_from_session(session_id);
    if (username.empty()) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザー情報が取得できません";
        return crow::response(400, error_response);
    }
    
    lock_guard<mutex> lock(mmtex);
    
    auto data = crow::json::load(req.body);
    string filename = data["filename"].s();
    
    // ファイル名の安全性をチェック
    if (filename.find("..") != string::npos || filename.find("/") != string::npos || filename.find("\\") != string::npos) {
        crow::json::wvalue error_response;
        error_response["error"] = "無効なファイル名です";
        return crow::response(400, error_response);
    }
    
    string file_path = get_user_memo_path(username) + filename;
    
    if (!filesystem::exists(file_path)) {
        crow::json::wvalue error_response;
        error_response["error"] = "ファイルが存在しません";
        return crow::response(404, error_response);
    }
    
    if (!filesystem::remove(file_path)) {
        crow::json::wvalue error_response;
        error_response["error"] = "ファイルの削除に失敗しました";
        return crow::response(500, error_response);
    }
    
    return crow::response(200);
}

inline crow::response memo_rename(const crow::request &req) {
    string session_id = MIDDLEWARE::extract_session_id(req);
    if (session_id.empty() || !AUTH::validate_session(session_id)) {
        crow::json::wvalue error_response;
        error_response["error"] = "認証が必要です";
        return crow::response(401, error_response);
    }
    
    string username = AUTH::get_username_from_session(session_id);
    if (username.empty()) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザー情報が取得できません";
        return crow::response(400, error_response);
    }
    
    lock_guard<mutex> lock(mmtex);
    
    auto data = crow::json::load(req.body);
    string old_filename = data["old_filename"].s();
    string new_stem = data["new_stem"].s();
    
    // ファイル名の安全性をチェック
    if (old_filename.find("..") != string::npos || old_filename.find("/") != string::npos || old_filename.find("\\") != string::npos ||
        new_stem.find("..") != string::npos || new_stem.find("/") != string::npos || new_stem.find("\\") != string::npos) {
        crow::json::wvalue error_response;
        error_response["error"] = "無効なファイル名です";
        return crow::response(400, error_response);
    }
    
    // 新しいファイル名を作成
    string new_filename = new_stem + ".json";
    
    string old_path = get_user_memo_path(username) + old_filename;
    string new_path = get_user_memo_path(username) + new_filename;
    
    if (!filesystem::exists(old_path)) {
        crow::json::wvalue error_response;
        error_response["error"] = "元のファイルが存在しません";
        return crow::response(404, error_response);
    }
    
    if (filesystem::exists(new_path)) {
        crow::json::wvalue error_response;
        error_response["error"] = "新しいファイル名が既に存在します";
        return crow::response(409, error_response);
    }
    
    try {
        filesystem::rename(old_path, new_path);
    } catch (...) {
        crow::json::wvalue error_response;
        error_response["error"] = "ファイル名の変更に失敗しました";
        return crow::response(500, error_response);
    }
    
    // 成功レスポンスに新しいファイル名情報を含める
    crow::json::wvalue ret;
    ret["new_filename"] = new_filename;
    ret["new_stem"] = new_stem;
    ret["extension"] = ".json";
    return crow::response(200, std::move(ret));
}

inline crow::response memo_update_tags(const crow::request &req) {
    string session_id = MIDDLEWARE::extract_session_id(req);
    if (session_id.empty() || !AUTH::validate_session(session_id)) {
        crow::json::wvalue error_response;
        error_response["error"] = "認証が必要です";
        return crow::response(401, error_response);
    }
    
    string username = AUTH::get_username_from_session(session_id);
    if (username.empty()) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザー情報が取得できません";
        return crow::response(400, error_response);
    }
    
    lock_guard<mutex> lock(mmtex);
    
    auto data = crow::json::load(req.body);
    string filename = data["filename"].s();
    vector<string> new_tags;
    
    // 新しいタグを読み込み
    if (data.has("tags")) {
        auto tags_array = data["tags"];
        for (size_t i = 0; i < tags_array.size(); i++) {
            new_tags.push_back(tags_array[i].s());
        }
    }
    
    // ファイル名の安全性をチェック
    if (filename.find("..") != string::npos || filename.find("/") != string::npos || filename.find("\\") != string::npos) {
        crow::json::wvalue error_response;
        error_response["error"] = "無効なファイル名です";
        return crow::response(400, error_response);
    }
    
    string file_path = get_user_memo_path(username) + filename;
    
    if (!filesystem::exists(file_path)) {
        crow::json::wvalue error_response;
        error_response["error"] = "ファイルが存在しません";
        return crow::response(404, error_response);
    }
    
    // 既存のメモデータを読み込み
    MemoData memo = load_memo_data(file_path);
    
    // タグを更新
    memo.tags = new_tags;
    memo.updated_at = get_current_timestamp();
    
    // 保存
    if (!save_memo_data(file_path, memo)) {
        crow::json::wvalue error_response;
        error_response["error"] = "タグの更新に失敗しました";
        return crow::response(500, error_response);
    }
    
    return crow::response(200);
}

inline crow::response memo_get_formats(const crow::request &req) {
    string session_id = MIDDLEWARE::extract_session_id(req);
    if (session_id.empty() || !AUTH::validate_session(session_id)) {
        crow::json::wvalue error_response;
        error_response["error"] = "認証が必要です";
        return crow::response(401, error_response);
    }
    
    crow::json::wvalue::list formats_list;
    for (const auto& format : supported_formats) {
        formats_list.push_back(format);
    }
    
    crow::json::wvalue ret;
    ret["formats"] = std::move(formats_list);
    return crow::response(200, std::move(ret));
}

inline crow::response memo_check_title(const crow::request &req) {
    string session_id = MIDDLEWARE::extract_session_id(req);
    if (session_id.empty() || !AUTH::validate_session(session_id)) {
        crow::json::wvalue error_response;
        error_response["error"] = "認証が必要です";
        return crow::response(401, error_response);
    }
    
    string username = AUTH::get_username_from_session(session_id);
    if (username.empty()) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザー情報が取得できません";
        return crow::response(400, error_response);
    }
    
    lock_guard<mutex> lock(mmtex);
    
    auto data = crow::json::load(req.body);
    string title = data["title"].s();
    
    // タイトルの安全性をチェック
    if (title.find("..") != string::npos || title.find("/") != string::npos || title.find("\\") != string::npos) {
        crow::json::wvalue error_response;
        error_response["error"] = "無効なタイトルです";
        return crow::response(400, error_response);
    }
    
    // タイトルが空でないことをチェック
    if (title.empty() || is_whitespace_only(title)) {
        crow::json::wvalue error_response;
        error_response["error"] = "タイトルは必須です";
        return crow::response(400, error_response);
    }
    
    // ファイル名として使用可能かチェック
    string filename = title + ".json";
    bool is_available = is_filename_unique(username, filename);
    
    crow::json::wvalue ret;
    ret["available"] = is_available;
    ret["title"] = title;
    if (!is_available) {
        ret["error"] = "このタイトルは既に使用されています";
    }
    return crow::response(200, std::move(ret));
}

inline crow::response memo_create_with_title(const crow::request &req) {
    string session_id = MIDDLEWARE::extract_session_id(req);
    if (session_id.empty() || !AUTH::validate_session(session_id)) {
        crow::json::wvalue error_response;
        error_response["error"] = "認証が必要です";
        return crow::response(401, error_response);
    }
    
    string username = AUTH::get_username_from_session(session_id);
    if (username.empty()) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザー情報が取得できません";
        return crow::response(400, error_response);
    }
    
    lock_guard<mutex> lock(mmtex);
    
    auto data = crow::json::load(req.body);
    string title = data["title"].s();
    vector<string> tags;
    string format = "txt"; // デフォルトはtxt
    
    // タイトルの安全性をチェック
    if (title.find("..") != string::npos || title.find("/") != string::npos || title.find("\\") != string::npos) {
        crow::json::wvalue error_response;
        error_response["error"] = "無効なタイトルです";
        return crow::response(400, error_response);
    }
    
    // タイトルが空でないことをチェック
    if (title.empty() || is_whitespace_only(title)) {
        crow::json::wvalue error_response;
        error_response["error"] = "タイトルは必須です";
        return crow::response(400, error_response);
    }
    
    // タグを読み込み
    if (data.has("tags")) {
        auto tags_array = data["tags"];
        for (size_t i = 0; i < tags_array.size(); i++) {
            tags.push_back(tags_array[i].s());
        }
    }
    
    // 形式を読み込み
    if (data.has("format")) {
        format = data["format"].s();
        if (!is_valid_format(format)) {
            crow::json::wvalue error_response;
            error_response["error"] = "無効な形式です";
            return crow::response(400, error_response);
        }
    }
    
    // ユーザーディレクトリを確保
    if (!ensure_user_directory(username)) {
        crow::json::wvalue error_response;
        error_response["error"] = "ユーザーディレクトリの作成に失敗しました";
        return crow::response(500, error_response);
    }
    
    // ファイル名を作成
    string filename = title + ".json";
    
    // ファイル名の一意性をチェック
    if (!is_filename_unique(username, filename)) {
        crow::json::wvalue error_response;
        error_response["error"] = "このタイトルは既に使用されています";
        return crow::response(409, error_response);
    }
    
    string file_path = get_user_memo_path(username) + filename;
    string timestamp = get_current_timestamp();
    
    // 新しいメモデータを作成
    MemoData memo;
    memo.tags = tags;
    memo.data = "";
    memo.format = format;
    memo.created_at = timestamp;
    memo.updated_at = timestamp;
    
    // JSONファイルとして保存
    if (!save_memo_data(file_path, memo)) {
        crow::json::wvalue error_response;
        error_response["error"] = "メモの作成に失敗しました";
        return crow::response(500, error_response);
    }
    
    crow::json::wvalue ret;
    ret["filename"] = filename;
    ret["stem"] = title;
    ret["extension"] = ".json";
    ret["format"] = format;
    ret["tags"] = tags;
    ret["data"] = "";
    ret["created_at"] = timestamp;
    ret["updated_at"] = timestamp;
    return crow::response(200, std::move(ret));
}

}//namespace MEMO