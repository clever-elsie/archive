#include <app/memo/data_structure.hpp>
#include <app/memo/helper.hpp>

namespace MEMO{

MemoData MemoData::load(const string& file_path) {
  MemoData memo;
  ifstream ifs(file_path);
  if(!ifs) return memo;
  memo.path = file_path;
  string json_str{istreambuf_iterator<char>(ifs), istreambuf_iterator<char>()};
  auto json_data = crow::json::load(json_str);
  if(!json_data) return memo;

  if (json_data.has("tag")) {
    auto tag_array = json_data["tag"];
    for (size_t i = 0; i < tag_array.size(); i++)
      memo.tag.insert(tag_array[i].s());
  }
  if (json_data.has("data")) memo.data = json_data["data"].s();
  if (json_data.has("format")) memo.format = json_data["format"].s();
  else memo.format = "txt";  // デフォルトはtxt
  if (json_data.has("created_at")) memo.created_at = json_data["created_at"].s();
  if (json_data.has("updated_at")) memo.updated_at = json_data["updated_at"].s();
  return memo;
}

bool MemoData::save(const string& file_path) {
  crow::json::wvalue json_data;
  json_data["tag"]=crow::json::wvalue::list(tag.begin(),tag.end());
  json_data["data"] = data;
  json_data["format"] = format;
  json_data["created_at"] = created_at;
  json_data["updated_at"] = updated_at;
  ofstream ofs(file_path);
  if (!ofs) return false;
  ofs << json_data.dump();
  return true;
}

string create_shared_memo(const string& title, const string& body, const string& author) {
  lock_guard<mutex> lock(shared_memo_mutex);
  string timestamp = get_current_timestamp();
  SharedMemoData memo{generate_unique_id(), title, body, timestamp, timestamp, author};
  shared_memos[memo.id] = memo;
  return memo.id;
}

bool update_shared_memo(const string& id, const string& title, const string& body) {
  lock_guard<mutex> lock(shared_memo_mutex);
  auto it = shared_memos.find(id);
  if (it == shared_memos.end()) return false;
  auto& memo = it->second;
  memo.title = title;
  memo.body = body;
  memo.updated_at = get_current_timestamp();
  return true;
}

bool delete_shared_memo(const string& id) {
  lock_guard<mutex> lock(shared_memo_mutex);
  auto it = shared_memos.find(id);
  if (it == shared_memos.end()) return false;
  shared_memos.erase(it);
  return true;
}

SharedMemoData get_shared_memo(const string& id) {
  lock_guard<mutex> lock(shared_memo_mutex);
  auto it = shared_memos.find(id);
  if (it == shared_memos.end()) return SharedMemoData();
  return it->second;
}

vector<SharedMemoData> get_all_shared_memos() {
  lock_guard<mutex> lock(shared_memo_mutex);
  vector<SharedMemoData> ret;
  ret.reserve(shared_memos.size());
  for (const auto& [id, memo] : shared_memos)
    ret.push_back(memo);
  return ret;
}
} // namespace MEMO