#pragma once
#include "../../headers.hpp"
#include <ranges>
#include <string_view>
#include <array>

namespace MEMO{
using namespace std;
// メモのJSON構造
struct MemoData {
  set<string> tag;
  string data;
  string format;  // "md", "txt", "json"のいずれか
  string created_at;
  string updated_at;
  string path; // 追加: メモのファイルパス
  bool save(const string& file_path); // MemoDataをJSONとして保存
  static MemoData load(const string& file_path); // JSONからMemoDataを読み込み
};

// 共用メモの構造体
struct SharedMemoData {
  string id;
  string title;
  string body;
  string created_at;
  string updated_at;
  string author; // 作成者
};

// グローバル変数
inline string memo_base_path;
inline mutex mmtex;
inline map<string, SharedMemoData> shared_memos; // 共用メモの保存
inline mutex shared_memo_mutex;

// サポートされているデータ形式
constexpr inline array<string_view, 3> sort_supported_formats(){
  array<string_view, 3> ret = {"md", "txt", "json"};
  std::ranges::sort(ret);
  return ret;
}
constexpr inline array<string_view, 3> supported_formats = sort_supported_formats();

string create_shared_memo(const string& title, const string& body, const string& author); // 共用メモの作成
bool update_shared_memo(const string& id, const string& title, const string& body); // 共用メモの更新
bool delete_shared_memo(const string& id); // 共用メモの削除
SharedMemoData get_shared_memo(const string& id); // 共用メモの取得
vector<SharedMemoData> get_all_shared_memos(); // 全共用メモの取得

}