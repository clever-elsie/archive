#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <app/viewer/model/types.hpp>

namespace VIEWER {

const char* media_type_name(MediaType type) noexcept;

struct ObservedFile {
  std::filesystem::path relative_path;
  std::filesystem::path canonical_relative_path;
  std::string name;
  MediaType media_type = MediaType::unknown;
  std::int64_t updated_at = 0;
  bool symlink = false;
  bool alias_only = false;
  bool archive = false;
};

struct ObservedDirectory {
  std::filesystem::path relative_path;
  std::filesystem::path canonical_relative_path;
  std::string name;
  std::string sort_key;
  std::int64_t updated_at = 0;
  bool symlink = false;
  bool alias_only = false;
  bool inaccessible = false;
  bool has_unsupported_files = false;
  bool has_metadata = false;
  DeclaredNodeKind declared_kind = DeclaredNodeKind::unspecified;
  std::optional<EntryId> persisted_id;
  std::vector<std::string> tags;
  std::vector<std::string> legacy_info_tags;
  std::vector<ObservedDirectory> children;
  std::vector<ObservedFile> files;
};

struct ScanDiagnostic {
  enum class Level : std::uint8_t { warning, error };
  Level level = Level::warning;
  std::filesystem::path relative_path;
  std::string message;
};

struct ScanSnapshot {
  std::filesystem::path root;
  ObservedDirectory tree;
  std::vector<ScanDiagnostic> diagnostics;
  bool complete = false;
};

} // namespace VIEWER
