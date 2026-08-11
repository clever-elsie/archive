#include <app/viewer/api/query.hpp>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string_view>

#include <app/viewer/api/entry_json.hpp>
#include <app/viewer/ordering.hpp>

namespace VIEWER::api {
namespace {

int group_rank(const NodeRecord& node) {
  if (node.kind == NodeKind::collection || node.kind == NodeKind::work) return 0;
  switch (node.media_type) {
    case MediaType::image: return 1;
    case MediaType::video: return 2;
    case MediaType::audio: return 3;
    case MediaType::text: return 4;
    case MediaType::document: return 5;
    default: return 6;
  }
}

bool grouped_sort(const crow::request& req) {
  const auto* grouping = req.url_params.get("grouping");
  return grouping && (std::string_view(grouping) == "media_type" || std::string_view(grouping) == "grouped");
}

std::string_view filter_value(const crow::request& req) {
  const auto* value = req.url_params.get("filter");
  return value ? std::string_view(value) : std::string_view{};
}

} // namespace

bool query_flag(const crow::request& req, const char* name) {
  const auto* value = req.url_params.get(name);
  if (!value) return false;
  const std::string_view text(value);
  return text == "1" || text == "true" || text == "yes";
}

std::size_t query_size(const crow::request& req, const char* name, std::size_t fallback, std::size_t maximum) {
  const auto* value = req.url_params.get(name);
  if (!value) return fallback;
  try {
    const auto parsed = std::stoull(value);
    return std::min<std::size_t>(parsed, maximum);
  } catch (...) {
    return fallback;
  }
}

std::size_t media_cache_index(std::string_view filter) {
  if (filter == "image" || filter == "images") return 1;
  if (filter == "video" || filter == "movies") return 2;
  if (filter == "audio" || filter == "musics") return 3;
  if (filter == "text" || filter == "texts") return 4;
  if (filter == "document" || filter == "pdfs") return 5;
  return 0;
}

void sort_nodes(const GraphState& state, std::vector<NodeRef>& refs, const crow::request& req) {
  const auto* key = req.url_params.get("sort_key");
  const auto* direction = req.url_params.get("direction");
  const bool updated = key && std::string_view(key) == "updated_at";
  const bool descending = direction && std::string_view(direction) == "desc";
  const bool by_media_type = grouped_sort(req);
  std::ranges::sort(refs, [&](NodeRef a, NodeRef b) {
    const auto* na = state.get(a);
    const auto* nb = state.get(b);
    if (!na || !nb) return a < b;
    if (by_media_type && group_rank(*na) != group_rank(*nb)) {
      return group_rank(*na) < group_rank(*nb);
    }
    if (updated && na->updated_at != nb->updated_at)
      return descending ? na->updated_at > nb->updated_at : na->updated_at < nb->updated_at;
    const auto name_comparison = ordering::compare_path(state.text(na->name), state.text(nb->name));
    if (name_comparison != 0) return descending ? name_comparison > 0 : name_comparison < 0;
    const auto path_comparison = ordering::compare_path(state.text(na->relative_path), state.text(nb->relative_path));
    if (path_comparison != 0) return descending ? path_comparison > 0 : path_comparison < 0;
    return descending ? na->id > nb->id : na->id < nb->id;
  });
}

crow::json::wvalue page_json(const Manager& manager, const ReadView& view,
                             std::vector<NodeRef> refs, const crow::request& req, bool administrator,
                             bool sort, std::optional<std::size_t> forced_limit) {
  const auto filter = filter_value(req);
  if (!filter.empty() && filter != "all") {
    std::erase_if(refs, [&](NodeRef ref) { return !matches_filter(view.state(), ref, filter); });
  }
  if (sort) sort_nodes(view.state(), refs, req);
  const auto page = query_size(req, "page", 0, 1'000'000);
  const auto limit = forced_limit.value_or(query_size(req, "limit", 50, 500));
  const auto begin = std::min(page * limit, refs.size());
  const auto end = std::min(begin + limit, refs.size());
  crow::json::wvalue::list items;
  for (std::size_t i = begin; i < end; ++i)
    items.emplace_back(node_json(manager, view.state(), refs[i], administrator));
  crow::json::wvalue result;
  result["items"] = std::move(items);
  result["page"] = page;
  result["limit"] = limit;
  result["has_next"] = end < refs.size();
  result["total"] = refs.size();
  if (const auto* grouping = req.url_params.get("grouping")) result["grouping"] = std::string(grouping);
  return result;
}

std::string lower_copy(std::string value) {
  for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}

bool contains_query(const GraphState& state, const NodeRecord& node, std::string_view query) {
  const auto q = lower_copy(std::string(query));
  const auto contains = [&](Slice slice) {
    return lower_copy(std::string(state.text(slice))).find(q) != std::string::npos;
  };
  if (contains(node.name) || contains(node.relative_path)) return true;
  const auto found = state.tags.find(node.id);
  if (found != state.tags.end()) {
    for (const auto& tag : found->second)
      if (lower_copy(tag).find(q) != std::string::npos) return true;
  }
  return false;
}

bool matches_filter(const GraphState& state, NodeRef ref, std::string_view filter) {
  const auto* node = state.get(ref);
  if (!node) return false;
  if (filter.empty() || filter == "all") return true;
  if (filter == "images") filter = "image";
  else if (filter == "movies") filter = "video";
  else if (filter == "musics") filter = "audio";
  else if (filter == "texts") filter = "text";
  else if (filter == "pdfs") filter = "document";
  if (filter == "collection" || filter == "directory") return node->kind == NodeKind::collection;
  if (filter == "work") return node->kind == NodeKind::work;
  const auto matches_type = [&](MediaType type) {
    return filter == media_type_name(type);
  };
  for (const auto type : {MediaType::image, MediaType::video, MediaType::audio,
                          MediaType::text, MediaType::document}) {
    if (!matches_type(type)) continue;
    if (node->kind == NodeKind::work || node->kind == NodeKind::collection)
      return (node->media_mask & media_type_bit(type)) != 0;
    return node->media_type == type;
  }
  return false;
}

} // namespace VIEWER::api
