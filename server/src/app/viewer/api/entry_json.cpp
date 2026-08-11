#include <app/viewer/api/entry_json.hpp>

#include <algorithm>
#include <array>
#include <utility>

#include <app/viewer/api/common.hpp>
#include <app/viewer/api/query.hpp>
#include <app/viewer/ordering.hpp>

namespace VIEWER::api {
namespace {

const char* kind_name(NodeKind kind) noexcept {
  switch (kind) {
    case NodeKind::collection: return "collection";
    case NodeKind::work: return "work";
    case NodeKind::media_set: return "media_set";
    case NodeKind::member: return "member";
    case NodeKind::edge: return "edge";
  }
  return "unknown";
}

std::string string_value(const GraphState& state, Slice slice) {
  return std::string(state.text(slice));
}

std::string id_value(EntryId id) {
  return std::to_string(id);
}

std::uint32_t visible_media_mask(const Manager& manager, const GraphState& state,
                                 NodeRef ref, bool administrator) {
  const auto* node = state.get(ref);
  if (!node || !manager.can_access(state, ref, administrator)) return 0;
  if (node->kind == NodeKind::media_set || node->kind == NodeKind::member)
    return node->media_mask;
  std::uint32_t result = 0;
  const auto begin = node->first_child;
  const auto end = begin + node->child_count;
  for (std::uint32_t index = begin; index < end && index < state.arena.child_refs.size(); ++index)
    result |= visible_media_mask(manager, state, state.arena.child_refs[index], administrator);
  return result;
}

void media_types_json(std::uint32_t mask, crow::json::wvalue& result) {
  crow::json::wvalue::list values;
  constexpr std::array types{MediaType::image, MediaType::video, MediaType::audio,
                             MediaType::text, MediaType::document};
  for (const auto type : types)
    if ((mask & media_type_bit(type)) != 0) values.push_back(std::string(media_type_name(type)));
  result = std::move(values);
}

crow::json::wvalue tag_list(const GraphState& state, EntryId id) {
  const auto found = state.tags.find(id);
  if (found == state.tags.end()) return crow::json::wvalue::list();
  return crow::json::wvalue::list(found->second.begin(), found->second.end());
}

} // namespace

const AliasRecord* find_alias(const GraphState& state, EntryId id) {
  const auto found = std::ranges::find_if(state.arena.aliases, [&](const auto& alias) { return alias.id == id; });
  return found == state.arena.aliases.end() ? nullptr : &*found;
}

crow::json::wvalue alias_json(const GraphState& state, const AliasRecord& alias) {
  crow::json::wvalue result;
  result["id"] = id_value(alias.id);
  result["kind"] = "edge";
  result["edge_kind"] = alias.symlink ? "symlink_alias" : "duplicate_alias";
  result["parent_id"] = id_value(alias.parent_id);
  result["alias_of"] = id_value(alias.canonical_id);
  result["hidden"] = true;
  result["source_path"] = std::string(state.text(alias.source_path));
  result["canonical_path"] = std::string(state.text(alias.canonical_path));
  return result;
}

void add_hidden_aliases(const Manager& manager, const GraphState& state, EntryId parent_id,
                       bool administrator, const crow::request& req, crow::json::wvalue& data) {
  if (!query_flag(req, "include_hidden")) return;
  crow::json::wvalue::list aliases;
  for (const auto& alias : state.arena.aliases) {
    if (alias.parent_id != parent_id || !manager.can_access_alias(state, alias, administrator)) continue;
    aliases.push_back(alias_json(state, alias));
  }
  data["hidden_aliases"] = std::move(aliases);
}

crow::json::wvalue node_json(const Manager& manager, const GraphState& state, NodeRef ref, bool administrator) {
  const auto* node = state.get(ref);
  crow::json::wvalue result;
  if (!node) return result;
  const auto name = ordering::display_name(state.text(node->name));
  result["id"] = id_value(node->id);
  result["kind"] = kind_name(node->kind);
  result["display_name"] = name.base;
  if (!name.ruby.empty()) result["display_name_ruby"] = name.ruby;
  result["state"] = "available";
  result["updated_at"] = std::to_string(node->updated_at);
  result["parent_id"] = id_value(0);
  if (node->parent != invalid_node) {
    if (const auto* parent = state.get(node->parent)) result["parent_id"] = id_value(parent->id);
  }
  const bool attached_media = (node->flags & node_attached_media_flag) != 0;
  if (attached_media) result["attached_media"] = true;
  result["capabilities"]["read"] = true;
  result["capabilities"]["edit_tags"] = administrator &&
      (node->kind == NodeKind::work || node->kind == NodeKind::collection) && !attached_media;
  if (node->kind == NodeKind::work || node->kind == NodeKind::collection) {
    media_types_json(visible_media_mask(manager, state, ref, administrator), result["media_types"]);
    if (node->kind == NodeKind::work && node->preview_member_id != 0) {
      const auto preview = state.index.find(node->preview_member_id);
      if (preview != state.index.end() && preview->second.node != invalid_node &&
          manager.can_access(state, preview->second.node, administrator))
        result["preview_member_id"] = id_value(node->preview_member_id);
    }
    result["tags"] = tag_list(state, node->id);
  }
  if (node->kind == NodeKind::media_set) {
    result["media_type"] = media_type_name(node->media_type);
    result["appendable"] = node->media_type == MediaType::text || node->media_type == MediaType::document;
    if (node->preview_member_id != 0) {
      const auto preview = state.index.find(node->preview_member_id);
      if (preview != state.index.end() && preview->second.node != invalid_node &&
          manager.can_access(state, preview->second.node, administrator))
        result["preview_member_id"] = id_value(node->preview_member_id);
    }
  }
  if (node->kind == NodeKind::member) {
    result["media_type"] = media_type_name(node->media_type);
    result["mime_type"] = string_value(state, node->mime_type);
    result["order"] = node->order;
    result["preview"] = (node->flags & node_preview_flag) != 0;
    result["content"]["href"] = "/req/viewer/content/" + std::to_string(node->id);
    result["content"]["supports_range"] = node->media_type == MediaType::video || node->media_type == MediaType::audio;
  }
  return result;
}

crow::response entry_response(const crow::request& req, const Manager& manager,
                             const ReadView& view, EntryId id, bool administrator) {
  const auto found = view.state().index.find(id);
  if (found == view.state().index.end()) return error_response(req, 404, "STALE_REFERENCE", "entry is not in the current graph", "root");
  if (found->second.alias) {
    const auto* alias = find_alias(view.state(), id);
    const auto canonical = view.state().index.find(found->second.canonical_id);
    if (!alias || canonical == view.state().index.end() || canonical->second.node == invalid_node ||
        !manager.can_access_alias(view.state(), *alias, administrator))
      return error_response(req, 403, "FORBIDDEN", "entry is not accessible");
    auto data = node_json(manager, view.state(), canonical->second.node, administrator);
    data["resolved_from_id"] = id_value(id);
    add_hidden_aliases(manager, view.state(), found->second.canonical_id, administrator, req, data);
    return crow::response(envelope(req, std::move(data)));
  }
  if (!manager.can_access(view.state(), found->second.node, administrator))
    return error_response(req, 403, "FORBIDDEN", "entry is not accessible");
  auto data = node_json(manager, view.state(), found->second.node, administrator);
  add_hidden_aliases(manager, view.state(), id, administrator, req, data);
  return crow::response(envelope(req, std::move(data)));
}

} // namespace VIEWER::api
