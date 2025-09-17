#include <app/memo/helper.hpp>

namespace MEMO{
using namespace std;

string generate_unique_id() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto time_t = system_clock::to_time_t(now);
  auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  stringstream ss;
  ss << put_time(localtime(&time_t), "%Y%m%d_%H%M%S");
  ss << "_" << setfill('0') << setw(3) << ms.count();
  return ss.str();
}

string generate_unique_filename(const string& username) {
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

crow::json::wvalue format_for_response(const filesystem::path& filepath, const MemoData&memo, bool header_only){
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

bool matches_search_query(const string& query, const string& title, const vector<string>& tags, const string& data) {
  if (query.empty()) return true; // 空のクエリはすべてにマッチ

  // クエリ，タイトル、タグ、データを小文字に変換
  string lower_query = query;
  string lower_title = title;
  string lower_data = data;
  vector<string> lower_tags;
  ranges::transform(lower_query, lower_query.begin(), ::tolower);
  ranges::transform(lower_title, lower_title.begin(), ::tolower);
  ranges::transform(lower_data, lower_data.begin(), ::tolower);
  for(string lower_tag : tags) {
    ranges::transform(lower_tag, lower_tag.begin(), ::tolower);
    lower_tags.push_back(lower_tag);
  }

  // 単純なAND検索（すべての単語が含まれているかチェック）
  stringstream ss(lower_query);
  string word;
  while (ss >> word) {
    bool found = false;
    // タイトルで検索
    if (lower_title.find(word) != string::npos) found = true;
    // タグで検索
    for (const auto& tag : lower_tags)
      if (tag.find(word) != string::npos) {
        found = true;
        break;
      }
    // データで検索
    if (lower_data.find(word) != string::npos)
      found = true;
    if (!found) {
      return false;  // 一つの単語でも見つからない場合はマッチしない
    }
  }
  return true;
}
} // namespace MEMO