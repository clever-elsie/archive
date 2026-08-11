#include <app/viewer/scan/metadata_reader.hpp>

#include <charconv>
#include <fstream>
#include <iterator>

#include <crow/json.h>

namespace VIEWER::scan {
namespace {

EntryId metadata_id(std::string_view value) {
  const auto first = value.data();
  const auto last = value.data() + value.size();
  EntryId numeric = 0;
  const auto parsed = std::from_chars(first, last, numeric);
  if (parsed.ec == std::errc{} && parsed.ptr == last && numeric != 0) return numeric;

  EntryId hash = 1469598103934665603ull;
  for (const auto byte : value) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= 1099511628211ull;
  }
  return hash == 0 ? 1 : hash;
}

} // namespace

void load_metadata(const std::filesystem::path& directory, ObservedDirectory& out,
                   std::vector<ScanDiagnostic>& diagnostics) {
  const auto metadata = directory / ".viewer.json";
  std::error_code ec;
  if (std::filesystem::is_regular_file(metadata, ec)) {
    out.has_metadata = true;
    std::ifstream stream(metadata);
    const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    const auto json = crow::json::load(text);
    if (!json) {
      diagnostics.push_back({ScanDiagnostic::Level::warning, out.relative_path, "invalid .viewer.json"});
    } else {
      try {
        if (json.has("id")) {
          if (json["id"].t() == crow::json::type::Number)
            out.persisted_id = static_cast<EntryId>(json["id"].u());
          else if (json["id"].t() == crow::json::type::String)
            out.persisted_id = metadata_id(std::string(json["id"].s()));
        }
        if (json.has("kind") && json["kind"].t() == crow::json::type::String) {
          const auto kind = json["kind"].s();
          if (kind == "collection") out.declared_kind = DeclaredNodeKind::collection;
          else if (kind == "work") out.declared_kind = DeclaredNodeKind::work;
          else if (kind == "media_set") out.declared_kind = DeclaredNodeKind::media_set;
          else diagnostics.push_back({ScanDiagnostic::Level::warning, out.relative_path, "unknown .viewer.json kind"});
        }
        if (json.has("tags")) {
          for (const auto& tag : json["tags"]) {
            if (tag.t() == crow::json::type::String) {
              out.tags.push_back(tag.s());
            } else if (tag.t() == crow::json::type::Object && tag.has("value") &&
                       tag["value"].t() == crow::json::type::String) {
              out.tags.push_back(tag["value"].s());
            }
          }
        }
      } catch (const std::exception& exception) {
        diagnostics.push_back({ScanDiagnostic::Level::warning, out.relative_path, exception.what()});
      }
    }
  } else {
    const auto legacy = directory / ".info";
    if (std::filesystem::is_regular_file(legacy, ec)) {
      out.has_metadata = true;
      std::ifstream stream(legacy);
      std::string line;
      while (std::getline(stream, line))
        if (!line.empty()) out.legacy_info_tags.push_back(line);
    }
  }
}

} // namespace VIEWER::scan
