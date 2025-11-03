#pragma once
#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>
#include <array>
#include <set>
#include <map>
#include <mutex>
#include <vector>
#include <filesystem>
#include <app/retrieve.hpp>
namespace MEMO{
using namespace std;
// メモのJSON構造
struct MemoData : public RETRIEVE::Retrieval{
  set<string> tag;
  string data;
  string format;  // "md", "txt", "json"のいずれか
  string created_at;
  string updated_at;
  string path; // 追加: メモのファイルパス
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