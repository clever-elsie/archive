#include <app/memo/data_structure.hpp>
#include <app/memo/helper.hpp>

#include <chrono>
#include <fstream>

namespace MEMO {

MemoData MemoData::load(const std::string& file_path) {
  MemoData memo;
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(file_path, ec);
  if (ec || status.type() == std::filesystem::file_type::symlink ||
      status.type() != std::filesystem::file_type::regular)
    return memo;

  std::ifstream input(file_path, std::ios::binary);
  if (!input)
    return memo;
  const std::string serialized{
      std::istreambuf_iterator<char>(input),
      std::istreambuf_iterator<char>()};
  const auto json = crow::json::load(serialized);
  if (!json)
    return memo;

  memo.path = file_path;
  if (json.has("tag") && json["tag"].t() == crow::json::type::List) {
    for (const auto& item : json["tag"]) {
      if (item.t() != crow::json::type::String)
        return MemoData();
      memo.tag.insert(item.s());
    }
  }
  if (json.has("data") && json["data"].t() == crow::json::type::String)
    memo.data = json["data"].s();
  if (json.has("format") && json["format"].t() == crow::json::type::String)
    memo.format = json["format"].s();
  if (!is_valid_format(memo.format))
    memo.format = "txt";
  if (json.has("created_at") &&
      json["created_at"].t() == crow::json::type::String)
    memo.created_at = json["created_at"].s();
  if (json.has("updated_at") &&
      json["updated_at"].t() == crow::json::type::String)
    memo.updated_at = json["updated_at"].s();
  memo.valid = true;
  return memo;
}

bool MemoData::save(const std::string& file_path) {
  if (!is_valid_format(format))
    return false;

  crow::json::wvalue json;
  json["tag"] = crow::json::wvalue::list(tag.begin(), tag.end());
  json["data"] = data;
  json["format"] = format;
  json["created_at"] = created_at;
  json["updated_at"] = updated_at;
  const std::string serialized = json.dump();

  const std::filesystem::path target(file_path);
  const auto parent = target.parent_path().empty()
                          ? std::filesystem::path(".")
                          : target.parent_path();
  const auto suffix =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto temporary =
      parent / (target.filename().string() + ".tmp." + std::to_string(suffix));

  bool written = false;
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (output) {
      output.write(
          serialized.data(),
          static_cast<std::streamsize>(serialized.size()));
      output.flush();
      written = static_cast<bool>(output);
    }
  }
  if (!written) {
    std::error_code cleanup_ec;
    std::filesystem::remove(temporary, cleanup_ec);
    return false;
  }

  std::error_code ec;
  std::filesystem::rename(temporary, target, ec);
  if (ec) {
    std::filesystem::remove(temporary, ec);
    return false;
  }
  return true;
}

std::string create_shared_memo(
    const std::string& title,
    const std::string& body,
    const std::string& author,
    bool author_is_admin) {
  std::lock_guard lock(shared_memo_mutex);
  const std::string id =
      std::to_string(shared_memo_next_id.fetch_add(1, std::memory_order_relaxed));
  const std::string timestamp = get_current_timestamp();
  shared_memos[id] = SharedMemoData{
      id, title, body, timestamp, timestamp, author, author_is_admin};
  return id;
}

bool update_shared_memo(
    const std::string& id,
    const std::string& title,
    const std::string& body,
    bool actor_is_admin) {
  std::lock_guard lock(shared_memo_mutex);
  const auto it = shared_memos.find(id);
  if (it == shared_memos.end() ||
      (it->second.author_is_admin && !actor_is_admin))
    return false;
  it->second.title = title;
  it->second.body = body;
  it->second.updated_at = get_current_timestamp();
  return true;
}

bool delete_shared_memo(const std::string& id, bool actor_is_admin) {
  std::lock_guard lock(shared_memo_mutex);
  const auto it = shared_memos.find(id);
  if (it == shared_memos.end() ||
      (it->second.author_is_admin && !actor_is_admin))
    return false;
  shared_memos.erase(it);
  return true;
}

SharedMemoData get_shared_memo(const std::string& id) {
  std::lock_guard lock(shared_memo_mutex);
  const auto it = shared_memos.find(id);
  return it == shared_memos.end() ? SharedMemoData{} : it->second;
}

std::vector<SharedMemoData> get_all_shared_memos() {
  std::lock_guard lock(shared_memo_mutex);
  std::vector<SharedMemoData> result;
  result.reserve(shared_memos.size());
  for (const auto& [id, memo] : shared_memos)
    result.push_back(memo);
  return result;
}

} // namespace MEMO
