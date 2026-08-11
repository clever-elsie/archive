#include <app/viewer/runtime/metadata_writer.hpp>

#include <fstream>
#include <iterator>
#include <unordered_map>

#include <crow/json.h>

namespace VIEWER::metadata {

bool write_tags(const std::filesystem::path& root, const NodeRecord& node,
                const GraphState& state, const std::vector<std::string>& tags) {
  const auto directory = root / std::filesystem::path(state.text(node.canonical_relative_path));
  std::error_code ec;
  if (!std::filesystem::is_directory(directory, ec)) return false;

  const auto target = directory / ".viewer.json";
  crow::json::wvalue json = crow::json::wvalue::empty_object();
  std::unordered_map<std::string, crow::json::wvalue> existing_tag_values;
  if (std::filesystem::is_regular_file(target, ec)) {
    std::ifstream input(target);
    if (!input) return false;
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const auto parsed = crow::json::load(text);
    if (!parsed || parsed.t() != crow::json::type::Object) return false;
    json = parsed;
    if (parsed.has("tags") && parsed["tags"].t() == crow::json::type::List) {
      for (const auto& old_tag : parsed["tags"]) {
        if (old_tag.t() == crow::json::type::Object && old_tag.has("value") &&
            old_tag["value"].t() == crow::json::type::String)
          existing_tag_values.emplace(old_tag["value"].s(), crow::json::wvalue(old_tag));
      }
    }
  }
  if (!json.has("schema_version")) json["schema_version"] = 1;
  if (!json.has("id")) json["id"] = std::to_string(node.id);
  if (!json.has("kind")) {
    switch (node.kind) {
      case NodeKind::collection: json["kind"] = "collection"; break;
      case NodeKind::work: json["kind"] = "work"; break;
      default: break;
    }
  }
  crow::json::wvalue::list serialized_tags;
  serialized_tags.reserve(tags.size());
  for (const auto& tag : tags) {
    if (const auto found = existing_tag_values.find(tag); found != existing_tag_values.end()) {
      serialized_tags.push_back(std::move(found->second));
    } else {
      crow::json::wvalue value;
      value["value"] = tag;
      serialized_tags.push_back(std::move(value));
    }
  }
  json["tags"] = std::move(serialized_tags);
  const auto temporary = directory / ".viewer.json.tmp";
  std::ofstream stream(temporary, std::ios::trunc);
  if (!stream) return false;
  stream << json.dump();
  stream.close();
  std::filesystem::rename(temporary, target, ec);
  if (ec) {
    std::filesystem::remove(temporary, ec);
    return false;
  }
  return true;
}

} // namespace VIEWER::metadata
