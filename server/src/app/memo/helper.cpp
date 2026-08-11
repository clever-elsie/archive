#include <app/memo/helper.hpp>

#include <cctype>
#include <ctime>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <system_error>

namespace MEMO {

namespace {

bool is_valid_utf8(const std::string& value) {
  for (std::size_t index = 0; index < value.size();) {
    const unsigned char first =
        static_cast<unsigned char>(value[index]);
    if (first < 0x80) {
      ++index;
      continue;
    }

    std::size_t length = 0;
    std::uint32_t codepoint = 0;
    if (first >= 0xc2 && first <= 0xdf) {
      length = 2;
      codepoint = first & 0x1f;
    } else if (first >= 0xe0 && first <= 0xef) {
      length = 3;
      codepoint = first & 0x0f;
    } else if (first >= 0xf0 && first <= 0xf4) {
      length = 4;
      codepoint = first & 0x07;
    } else {
      return false;
    }
    if (index + length > value.size())
      return false;
    for (std::size_t offset = 1; offset < length; ++offset) {
      const unsigned char continuation =
          static_cast<unsigned char>(value[index + offset]);
      if ((continuation & 0xc0) != 0x80)
        return false;
      codepoint = (codepoint << 6) | (continuation & 0x3f);
    }
    if ((length == 3 && codepoint < 0x800) ||
        (length == 4 && codepoint < 0x10000) ||
        codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff))
      return false;
    index += length;
  }
  return true;
}

bool no_symlink(const std::filesystem::path& path) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  return !ec && status.type() != std::filesystem::file_type::symlink;
}

bool path_is_inside(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate) {
  const auto normalized_root = root.lexically_normal();
  const auto normalized_candidate = candidate.lexically_normal();
  auto root_it = normalized_root.begin();
  auto candidate_it = normalized_candidate.begin();
  for (; root_it != normalized_root.end(); ++root_it, ++candidate_it) {
    if (candidate_it == normalized_candidate.end() || *root_it != *candidate_it)
      return false;
  }
  return true;
}

std::filesystem::path memo_root() {
  if (memo_base_path.empty()) return {};
  auto root = std::filesystem::path(memo_base_path).lexically_normal();
  // 設定値は互換性のため末尾の区切り文字を許すが、pathの反復では
  // 末尾の空要素として扱われるため、所属判定の前に取り除く。
  if (root != root.root_path() && root.filename().empty())
    root = root.parent_path();
  return root;
}

} // namespace

std::string get_user_memo_path(const std::string& username) {
  return (memo_root() / username).string() +
         std::filesystem::path::preferred_separator;
}

bool valid_filename_component(const std::string& filename) {
  if (filename.empty() || filename == "." || filename == ".." ||
      !is_valid_utf8(filename))
    return false;
  for (const unsigned char byte : filename) {
    if (byte < 0x20 || byte == 0x7f || byte == 0 ||
        byte == '<' || byte == '>' || byte == '&' ||
        byte == '"' || byte == '\'' || byte == 96 ||
        byte == '=' || byte == '/' || byte == '\\')
      return false;
  }
  return true;
}

std::optional<std::filesystem::path> safe_user_memo_path(
    const std::string& username,
    const std::string& filename) {
  if (memo_base_path.empty() || username.empty() ||
      !valid_filename_component(username) ||
      !valid_filename_component(filename))
    return std::nullopt;
  const auto root = memo_root();
  const auto candidate = (root / username / filename).lexically_normal();
  if (!path_is_inside(root, candidate))
    return std::nullopt;

  std::error_code ec;
  if (!std::filesystem::is_directory(root, ec) || ec || !no_symlink(root))
    return std::nullopt;
  if (!no_symlink(root / username))
    return std::nullopt;
  const auto candidate_status = std::filesystem::symlink_status(candidate, ec);
  // 新規作成・タイトル確認ではcandidateがまだ存在しない。ENOENTは
  // 「利用可能な未作成ファイル」を意味するため、パス検証の失敗にしない。
  if ((ec && ec != std::make_error_code(std::errc::no_such_file_or_directory)) ||
      candidate_status.type() == std::filesystem::file_type::symlink)
    return std::nullopt;
  return candidate;
}

bool ensure_user_directory(const std::string& username) {
  if (memo_base_path.empty() || username.empty() ||
      !valid_filename_component(username))
    return false;
  const auto root = memo_root();
  const auto user_path = (root / username).lexically_normal();
  if (!path_is_inside(root, user_path))
    return false;
  try {
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec) || ec || !no_symlink(root))
      return false;
    if (std::filesystem::exists(user_path, ec)) {
      return !ec && std::filesystem::is_directory(user_path, ec) &&
             !ec && no_symlink(user_path);
    }
    if (!std::filesystem::create_directory(user_path, ec) || ec)
      return false;
    return no_symlink(user_path);
  } catch (...) {
    return false;
  }
}

bool is_filename_unique(
    const std::string& username,
    const std::string& filename) {
  const auto path = safe_user_memo_path(username, filename);
  if (!path)
    return false;
  std::error_code ec;
  return !std::filesystem::exists(*path, ec) && !ec;
}

std::shared_ptr<std::mutex> user_memo_mutex(const std::string& username) {
  static std::mutex registry_mutex;
  static std::map<std::string, std::weak_ptr<std::mutex>> registry;
  std::lock_guard lock(registry_mutex);
  if (const auto it = registry.find(username); it != registry.end()) {
    if (auto existing = it->second.lock())
      return existing;
  }
  auto created = std::make_shared<std::mutex>();
  registry[username] = created;
  return created;
}

std::string generate_unique_id() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto time = system_clock::to_time_t(now);
  const auto elapsed_milliseconds = duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()) % 1000;
  std::tm local{};
  localtime_r(&time, &local);
  std::ostringstream stream;
  stream << std::put_time(&local, "%Y%m%d_%H%M%S")
         << "_" << std::setfill('0') << std::setw(3)
         << elapsed_milliseconds.count();
  return stream.str();
}

std::string generate_unique_filename(const std::string& username) {
  const std::string base = generate_unique_id();
  for (std::uint64_t counter = 0;
       counter < std::numeric_limits<std::uint64_t>::max();
       ++counter) {
    const std::string suffix =
        counter == 0 ? std::string() : "_" + std::to_string(counter);
    const std::string filename = base + suffix + ".json";
    if (is_filename_unique(username, filename))
      return filename;
  }
  return {};
}

std::string get_current_timestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm local{};
  localtime_r(&time, &local);
  std::ostringstream stream;
  stream << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
  return stream.str();
}

crow::json::wvalue format_for_response(
    const std::filesystem::path& filepath,
    const MemoData& memo,
    bool header_only) {
  crow::json::wvalue value;
  value["filename"] = filepath.filename().string();
  value["stem"] = filepath.stem().string();
  value["extension"] = filepath.extension().string();
  value["format"] = memo.format;
  value["tag"] = crow::json::wvalue::list(memo.tag.begin(), memo.tag.end());
  value["created_at"] = memo.created_at;
  value["updated_at"] = memo.updated_at;
  if (!header_only)
    value["data"] = memo.data;
  return value;
}

bool matches_search_query(
    const std::string& query,
    const std::string& title,
    const std::vector<std::string>& tags,
    const std::string& data) {
  if (query.empty())
    return true;
  auto lower = [](std::string value) {
    std::ranges::transform(value, value.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    return value;
  };
  const std::string lower_query = lower(query);
  const std::string lower_title = lower(title);
  const std::string lower_data = lower(data);
  std::vector<std::string> lower_tags;
  lower_tags.reserve(tags.size());
  for (const auto& tag : tags)
    lower_tags.push_back(lower(tag));
  std::stringstream stream(lower_query);
  std::string word;
  while (stream >> word) {
    bool found = lower_title.find(word) != std::string::npos ||
                 lower_data.find(word) != std::string::npos;
    if (!found) {
      for (const auto& tag : lower_tags) {
        if (tag.find(word) != std::string::npos) {
          found = true;
          break;
        }
      }
    }
    if (!found)
      return false;
  }
  return true;
}

} // namespace MEMO
