#pragma once
#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>
#include <array>
#include <atomic>
#include <set>
#include <map>
#include <mutex>
#include <memory>
#include <vector>
#include <filesystem>
#include <app/retrieve.hpp>
namespace MEMO{
using namespace std;
// メモのJSON構造
struct MemoData : public RETRIEVE::Retrieval{
  set<string> tag;
  string data;
  string format;  // "txt"または"json"
  string created_at;
  string updated_at;
  string path; // 追加: メモのファイルパス
  bool valid = false;
  MemoData()=default;
  template<class set_t, class string_t>
  MemoData(set_t&&tag_, string_t&&data_, const string&format_, const string&created_at_, const string&updated_at_, const string&path_)
  :tag(std::forward<set_t>(tag_)), data(std::forward<string_t>(data_)), format(format_), created_at(created_at_), updated_at(updated_at_), path(path_){}
  bool save(const string& file_path); // MemoDataをJSONとして保存
  static MemoData load(const string& file_path); // JSONからMemoDataを読み込み
  virtual bool match(const string&s)const override{
    return tag.contains(s) || path.contains(s);
  }
};

// 共用メモの構造体
struct SharedMemoData {
  string id;
  string title;
  string body;
  string created_at;
  string updated_at;
  string author; // 作成者
  bool author_is_admin = false;
};

// グローバル変数
inline string memo_base_path;
inline map<string, SharedMemoData> shared_memos; // 共用メモの保存
inline mutex shared_memo_mutex;
inline atomic<uint64_t> shared_memo_next_id{1};

// サポートされているデータ形式
constexpr inline array<string_view, 2> sort_supported_formats(){
  array<string_view, 2> ret = {"json", "txt"};
  std::ranges::sort(ret);
  return ret;
}
constexpr inline array<string_view, 2> supported_formats = sort_supported_formats();

shared_ptr<mutex> user_memo_mutex(const string& username);
string create_shared_memo(const string& title, const string& body, const string& author, bool author_is_admin); // 共用メモの作成
bool update_shared_memo(const string& id, const string& title, const string& body, bool actor_is_admin); // 共用メモの更新
bool delete_shared_memo(const string& id, bool actor_is_admin); // 共用メモの削除
SharedMemoData get_shared_memo(const string& id); // 共用メモの取得
vector<SharedMemoData> get_all_shared_memos(); // 全共用メモの取得

}
