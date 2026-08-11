#include <app/viewer/scanner.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <set>
#include <system_error>

#include <app/viewer/ordering.hpp>
#include <app/viewer/scan/classifier.hpp>
#include <app/viewer/scan/metadata_reader.hpp>

namespace VIEWER {
namespace {

namespace fs = std::filesystem;

std::int64_t file_time(const fs::path& path) {
  std::error_code ec;
  const auto value = fs::last_write_time(path, ec);
  if (ec) return 0;
  return value.time_since_epoch().count();
}

bool is_within(const fs::path& root, const fs::path& candidate) {
  auto r = root.begin();
  auto c = candidate.begin();
  for (; r != root.end() && c != candidate.end(); ++r, ++c)
    if (*r != *c) return false;
  return r == root.end();
}

bool cancelled(const std::atomic_bool* stop_requested) {
  return stop_requested && stop_requested->load(std::memory_order_acquire);
}

ObservedDirectory scan_directory(
    const fs::path& root,
    const fs::path& root_canonical,
    const fs::path& path,
    const fs::path& relative,
    const fs::path& canonical_relative,
    bool symlink,
    std::set<std::string>& visited,
    std::set<std::string>& visited_files,
    std::vector<ScanDiagnostic>& diagnostics,
    const std::atomic_bool* stop_requested) {
  ObservedDirectory result;
  if (cancelled(stop_requested)) return result;
  result.relative_path = relative;
  result.canonical_relative_path = canonical_relative;
  result.name = relative.empty() ? root.filename().generic_string() : relative.filename().generic_string();
  result.sort_key = ordering::sort_key(result.name);
  result.updated_at = file_time(path);
  result.symlink = symlink;

  const auto canonical_key = canonical_relative.generic_string();
  if (!visited.insert(canonical_key).second) {
    result.alias_only = true;
    return result;
  }

  scan::load_metadata(path, result, diagnostics);

  std::error_code ec;
  std::vector<fs::directory_entry> entries;
  for (fs::directory_iterator it(path, fs::directory_options::skip_permission_denied, ec), end;
       it != end; it.increment(ec)) {
    if (cancelled(stop_requested)) break;
    if (ec) {
      diagnostics.push_back({ScanDiagnostic::Level::warning, relative, ec.message()});
      ec.clear();
      continue;
    }
    entries.push_back(*it);
  }
  if (cancelled(stop_requested)) return result;
  std::ranges::sort(entries, [&](const auto& a, const auto& b) { return ordering::less_path(a.path(), b.path()); });

  for (const auto& entry : entries) {
    if (cancelled(stop_requested)) return result;
    const auto entry_path = entry.path();
    const auto entry_relative = relative / entry_path.filename();
    const bool entry_symlink = entry.is_symlink(ec);
    ec.clear();
    const auto canonical = fs::weakly_canonical(entry_path, ec);
    if (ec || !is_within(root_canonical, canonical)) {
      diagnostics.push_back({ScanDiagnostic::Level::warning, entry_relative,
                             "path is outside viewer root or cannot be resolved"});
      continue;
    }
    const auto canonical_rel = fs::relative(canonical, root_canonical, ec).lexically_normal();
    if (ec) continue;

    const bool directory = fs::is_directory(canonical, ec);
    ec.clear();
    if (directory) {
      result.children.push_back(scan_directory(root, root_canonical, entry_path, entry_relative, canonical_rel,
                                               entry_symlink, visited, visited_files, diagnostics, stop_requested));
      continue;
    }

    if (!fs::is_regular_file(canonical, ec)) continue;
    ec.clear();
    const auto links = fs::hard_link_count(canonical, ec);
    if (!ec && links > 1) {
      diagnostics.push_back({ScanDiagnostic::Level::warning, entry_relative, "hardlink ignored"});
      continue;
    }
    if (scan::is_metadata_file(entry_path)) continue;
    bool archive = false;
    const auto type = scan::classify(entry_path, archive);
    if (type == MediaType::unknown) {
      result.has_unsupported_files = true;
      diagnostics.push_back({ScanDiagnostic::Level::warning, entry_relative, "unsupported file ignored"});
      continue;
    }
    const bool alias_only = !visited_files.insert(canonical_rel.generic_string()).second;
    result.files.push_back({entry_relative, canonical_rel, entry_path.filename().generic_string(), type,
                            file_time(canonical), entry_symlink, alias_only, archive});
  }

  if (cancelled(stop_requested)) return result;

  // 旧Info実装と同様、既知のメディアも有効な子も持たない実ディレクトリは
  // 構造上の子として扱わない。空の子を残すと、メディアを持つ親まで葉で
  // なくなり、Work判定を誤ってCollection側へ送ってしまう。alias_onlyは
  // 実体が別の正規Entryにあるため、管理者向けの隠蔽情報として残す。
  std::erase_if(result.children, [](const ObservedDirectory& child) {
    return !child.alias_only && child.files.empty() && child.children.empty();
  });

  std::ranges::sort(result.children, [&](const auto& a, const auto& b) {
    return ordering::compare_path(a.relative_path.generic_string(), b.relative_path.generic_string()) < 0;
  });
  std::ranges::sort(result.files, [&](const auto& a, const auto& b) {
    return ordering::compare_path(a.relative_path.generic_string(), b.relative_path.generic_string()) < 0;
  });
  if (!result.legacy_info_tags.empty()) {
    const bool has_direct_image = std::ranges::any_of(result.files, [](const auto& file) {
      return file.media_type == MediaType::image;
    });
    if (has_direct_image) {
      result.tags = result.legacy_info_tags;
    } else {
      diagnostics.push_back({ScanDiagnostic::Level::warning, result.relative_path,
                             ".info ignored outside an image directory"});
      result.legacy_info_tags.clear();
    }
  }
  return result;
}

} // namespace

const char* media_type_name(MediaType type) noexcept {
  switch (type) {
    case MediaType::image: return "image";
    case MediaType::video: return "video";
    case MediaType::audio: return "audio";
    case MediaType::text: return "text";
    case MediaType::document: return "document";
    default: return "unknown";
  }
}

ScanSnapshot Scanner::scan(const fs::path& root, const std::atomic_bool* stop_requested) {
  ScanSnapshot snapshot;
  snapshot.root = root;
  if (cancelled(stop_requested)) return snapshot;
  std::error_code ec;
  const auto canonical_root = fs::weakly_canonical(root, ec);
  if (ec || !fs::is_directory(canonical_root, ec)) {
    snapshot.diagnostics.push_back({ScanDiagnostic::Level::error, {}, "viewer root is not accessible"});
    return snapshot;
  }
  std::set<std::string> visited;
  std::set<std::string> visited_files;
  snapshot.tree = scan_directory(canonical_root, canonical_root, canonical_root, {}, {}, false, visited, visited_files,
                                 snapshot.diagnostics, stop_requested);
  snapshot.complete = !cancelled(stop_requested);
  return snapshot;
}

} // namespace VIEWER
