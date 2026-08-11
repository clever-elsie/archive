#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <app/viewer/model/observation.hpp>

namespace VIEWER {

using NodeRef = std::uint32_t;
inline constexpr NodeRef invalid_node = static_cast<NodeRef>(-1);
inline constexpr std::uint32_t node_preview_flag = 1u;
inline constexpr std::uint32_t node_standalone_media_flag = 1u << 1;
inline constexpr std::uint32_t node_attached_media_flag = 1u << 2;

enum class NodeKind : std::uint8_t {
  collection,
  work,
  media_set,
  member,
  edge
};

struct Slice final {
  std::uint32_t offset = 0;
  std::uint32_t size = 0;
};

struct NodeRecord final {
  EntryId id = 0;
  NodeKind kind = NodeKind::collection;
  MediaType media_type = MediaType::unknown;
  NodeRef parent = invalid_node;
  std::uint32_t first_child = 0;
  std::uint32_t child_count = 0;
  std::uint32_t order = 0;
  std::int64_t updated_at = 0;
  EntryId preview_member_id = 0;
  // Work/Collectionが直接・間接に保持するメディア種別の固定ビット集合。
  // MediaSet/Memberでは自身の種別だけを保持する。
  std::uint32_t media_mask = 0;
  std::uint32_t flags = 0;
  Slice name;
  Slice relative_path;
  Slice canonical_relative_path;
  Slice mime_type;
  Slice source_path;
};

struct AliasRecord final {
  EntryId id = 0;
  EntryId canonical_id = 0;
  EntryId parent_id = 0;
  Slice source_path;
  Slice canonical_path;
  std::uint8_t symlink = 0;
};

struct Lookup final {
  NodeRef node = invalid_node;
  EntryId canonical_id = 0;
  bool alias = false;
};

struct Arena final {
  std::vector<NodeRecord> nodes;
  std::vector<NodeRef> child_refs;
  std::vector<AliasRecord> aliases;
  std::vector<char> strings;

  void clear() noexcept {
    nodes.clear();
    child_refs.clear();
    aliases.clear();
    strings.clear();
  }

  Slice store(std::string_view value) {
    const auto offset = static_cast<std::uint32_t>(strings.size());
    strings.insert(strings.end(), value.begin(), value.end());
    return {offset, static_cast<std::uint32_t>(value.size())};
  }

  std::string_view view(Slice slice) const noexcept {
    if (slice.offset > strings.size() || slice.size > strings.size() - slice.offset)
      return {};
    return {strings.data() + slice.offset, slice.size};
  }
};

struct GraphState final {
  Arena arena;
  std::unordered_map<EntryId, Lookup> index;
  std::unordered_map<EntryId, std::vector<std::string>> tags;
  std::vector<ScanDiagnostic> diagnostics;
  // Workと、混合ディレクトリ内の独立ファイルMemberを全種別・種別別に
  // 更新時刻順で保持するページ用キャッシュ。0番目は全種別、1番目以降は
  // MediaTypeの列挙順に対応する。
  std::array<std::vector<NodeRef>, 6> media_page_cache;
  NodeRef root = invalid_node;
  bool ready = false;

  void clear() {
    arena.clear();
    index.clear();
    tags.clear();
    diagnostics.clear();
    for (auto& cache : media_page_cache) cache.clear();
    root = invalid_node;
    ready = false;
  }

  const NodeRecord* get(NodeRef ref) const noexcept {
    return ref < arena.nodes.size() ? &arena.nodes[ref] : nullptr;
  }
  NodeRecord* get(NodeRef ref) noexcept {
    return ref < arena.nodes.size() ? &arena.nodes[ref] : nullptr;
  }
  std::string_view text(Slice slice) const noexcept { return arena.view(slice); }
};

} // namespace VIEWER
