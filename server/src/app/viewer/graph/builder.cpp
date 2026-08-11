#include <app/viewer/graph/builder.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <functional>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

#include <crow/logging.h>

#include <app/viewer/ordering.hpp>

namespace VIEWER {
namespace {

namespace fs = std::filesystem;

std::string lower_ascii(std::string value) {
  for (char& c : value)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}

std::string mime_for(std::string_view name) {
  const auto dot = name.rfind('.');
  const auto ext = dot == std::string_view::npos ? std::string{} : lower_ascii(std::string(name.substr(dot)));
  if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
  if (ext == ".png") return "image/png";
  if (ext == ".webp") return "image/webp";
  if (ext == ".gif") return "image/gif";
  if (ext == ".zip") return "application/zip";
  if (ext == ".mp4") return "video/mp4";
  if (ext == ".mkv") return "video/x-matroska";
  if (ext == ".webm") return "video/webm";
  if (ext == ".mp3") return "audio/mpeg";
  if (ext == ".wav") return "audio/wav";
  if (ext == ".flac") return "audio/flac";
  if (ext == ".txt") return "text/plain; charset=utf-8";
  if (ext == ".md") return "text/markdown; charset=utf-8";
  if (ext == ".pdf") return "application/pdf";
  return "application/octet-stream";
}

EntryId fnv1a(std::string_view value) {
  EntryId hash = 1469598103934665603ull;
  for (const unsigned char c : value) {
    hash ^= c;
    hash *= 1099511628211ull;
  }
  return hash == 0 ? 1 : hash;
}

std::string path_text(const fs::path& path) {
  return path.generic_string();
}

bool has_direct_media(const ObservedDirectory& directory) {
  return !directory.files.empty();
}

bool is_direct_image_directory(const ObservedDirectory& directory) {
  if (!directory.children.empty() || directory.has_unsupported_files) return false;
  bool has_image = false;
  for (const auto& file : directory.files) {
    if (file.media_type != MediaType::image) return false;
    has_image = true;
  }
  return has_image;
}

bool has_media_in_subtree(const ObservedDirectory& directory);

bool is_video_leaf_directory(const ObservedDirectory& directory) {
  if (directory.alias_only || directory.files.empty() || !directory.children.empty() ||
      directory.has_unsupported_files ||
      directory.declared_kind != DeclaredNodeKind::unspecified)
    return false;

  bool has_video = false;
  std::size_t image_count = 0;
  for (const auto& file : directory.files) {
    switch (file.media_type) {
      case MediaType::video:
        has_video = true;
        break;
      case MediaType::image:
        ++image_count;
        break;
      case MediaType::audio:
      case MediaType::text:
      case MediaType::document:
      case MediaType::unknown: return false;
    }
  }
  // 動画葉だけは任意名の画像1枚をプレビューMemberとして許可する。
  return has_video && image_count <= 1;
}

bool is_audio_leaf_directory(const ObservedDirectory& directory) {
  if (directory.alias_only || directory.files.empty() || !directory.children.empty() ||
      directory.has_unsupported_files ||
      directory.declared_kind != DeclaredNodeKind::unspecified)
    return false;
  return std::ranges::all_of(directory.files, [](const auto& file) {
    return file.media_type == MediaType::audio;
  });
}

bool is_direct_media_leaf_directory(const ObservedDirectory& directory) {
  if (directory.alias_only || directory.files.empty() || !directory.children.empty() ||
      directory.has_unsupported_files ||
      directory.declared_kind == DeclaredNodeKind::collection ||
      directory.declared_kind == DeclaredNodeKind::work)
    return false;
  return std::ranges::all_of(directory.files, [](const auto& file) {
    return file.media_type != MediaType::unknown;
  });
}

bool is_media_set_directory(const ObservedDirectory& directory) {
  if (directory.alias_only) return false;
  if (directory.declared_kind == DeclaredNodeKind::media_set)
    return is_direct_media_leaf_directory(directory);
  if (directory.declared_kind != DeclaredNodeKind::unspecified) return false;
  // 明示されていないMediaSetは、既存の動画葉または音声葉とする。
  // 動画だけは任意名の画像1枚を同じSetのプレビューとして許可する。
  return is_video_leaf_directory(directory) || is_audio_leaf_directory(directory);
}

bool has_only_direct_media_leaf_children(const ObservedDirectory& directory) {
  bool found_media_child = false;
  for (const auto& child : directory.children) {
    if (!has_media_in_subtree(child)) continue;
    found_media_child = true;
    if (!is_direct_media_leaf_directory(child)) return false;
  }
  return found_media_child;
}

bool has_only_media_set_children(const ObservedDirectory& directory) {
  bool found_media_child = false;
  for (const auto& child : directory.children) {
    // 未対応ファイルだけを持つディレクトリ（例: 管理用ディレクトリ）は、
    // Workの構造判定を妨げない。認識可能なメディアを持つ子だけを
    // MediaSetとして検証する。
    if (!has_media_in_subtree(child)) continue;
    found_media_child = true;
    if (!is_media_set_directory(child)) return false;
  }
  return found_media_child;
}

bool has_media_in_subtree(const ObservedDirectory& directory) {
  if (!directory.files.empty()) return true;
  return std::ranges::any_of(directory.children, has_media_in_subtree);
}

bool is_work_directory(const ObservedDirectory& directory) {
  if (directory.declared_kind == DeclaredNodeKind::collection ||
      directory.declared_kind == DeclaredNodeKind::media_set)
    return false;
  // 直接メディアだけを持つ葉ディレクトリだけをWorkとする。直接メディアと
  // 子ディレクトリが同居する場合は、メディアが付随したCollectionとして扱う。
  // 直接メディアを持たず、認識可能な子がすべてMediaSet候補である場合も、
  // それらをMediaSetとしてまとめるWorkとする。動画葉または音声葉と、
  // メディア葉を内包するディレクトリが混在する場合はCollectionとして扱う。
  if (directory.children.empty()) {
    return has_direct_media(directory) || directory.declared_kind == DeclaredNodeKind::work;
  }
  if (has_direct_media(directory)) return false;
  // kind=workは、葉ディレクトリだけを束ねる曖昧な境界の明示に限る。
  // 直接メディアと子ディレクトリの同居、または孫以下を含む構造を
  // 明示指定だけでWorkへ変えてはならない。
  if (directory.declared_kind == DeclaredNodeKind::work)
    return has_only_direct_media_leaf_children(directory);
  return has_only_media_set_children(directory);
}

void finalize_graph_impl(GraphState& state, const std::atomic_bool* stop_requested) {
  const auto cancelled = [&] {
    return stop_requested && stop_requested->load(std::memory_order_acquire);
  };

  // 子参照列から親参照と兄弟内の順序を確定する。child_refsを唯一の正規データ
  // とし、各NodeRecordには実行時に必要な導出情報を設定する。
  for (auto& node : state.arena.nodes) {
    node.parent = invalid_node;
    node.order = 0;
    node.preview_member_id = 0;
    node.media_mask = node.kind == NodeKind::member || node.kind == NodeKind::media_set
      ? media_type_bit(node.media_type)
      : 0;
  }
  for (NodeRef parent = 0; parent < state.arena.nodes.size(); ++parent) {
    if (cancelled()) return;
    auto& node = state.arena.nodes[parent];
    const auto begin = static_cast<std::size_t>(node.first_child);
    const auto end = begin + node.child_count;
    if (begin > state.arena.child_refs.size() || end > state.arena.child_refs.size()) return;
    for (std::size_t index = begin; index < end; ++index) {
      const auto child = state.arena.child_refs[index];
      if (child >= state.arena.nodes.size()) return;
      auto& child_node = state.arena.nodes[child];
      child_node.parent = parent;
      child_node.order = static_cast<std::uint32_t>(index - begin);
    }
  }

  std::vector<std::uint8_t> computed(state.arena.nodes.size(), 0);
  std::vector<std::uint8_t> visiting(state.arena.nodes.size(), 0);
  std::function<std::uint32_t(NodeRef)> compute_mask = [&](NodeRef ref) -> std::uint32_t {
    if (ref >= state.arena.nodes.size() || cancelled()) return 0;
    if (computed[ref] != 0) return state.arena.nodes[ref].media_mask;
    if (visiting[ref] != 0) return 0;
    visiting[ref] = 1;
    auto& node = state.arena.nodes[ref];
    if (node.kind != NodeKind::member && node.kind != NodeKind::media_set) {
      node.media_mask = 0;
      const auto begin = static_cast<std::size_t>(node.first_child);
      const auto end = begin + node.child_count;
      if (begin > state.arena.child_refs.size() || end > state.arena.child_refs.size()) return 0;
      for (std::size_t index = begin; index < end; ++index)
        node.media_mask |= compute_mask(state.arena.child_refs[index]);
    }
    visiting[ref] = 0;
    computed[ref] = 1;
    return node.media_mask;
  };
  for (NodeRef ref = 0; ref < state.arena.nodes.size(); ++ref) {
    if (cancelled()) return;
    compute_mask(ref);
  }

  std::vector<std::uint8_t> preview_visiting(state.arena.nodes.size(), 0);
  std::function<EntryId(NodeRef)> find_flagged_preview = [&](NodeRef ref) -> EntryId {
    if (ref >= state.arena.nodes.size() || cancelled()) return 0;
    if (preview_visiting[ref] != 0) return 0;
    preview_visiting[ref] = 1;
    const auto& node = state.arena.nodes[ref];
    if (node.kind == NodeKind::member) {
      preview_visiting[ref] = 0;
      return (node.flags & node_preview_flag) != 0 ? node.id : 0;
    }
    const auto begin = static_cast<std::size_t>(node.first_child);
    const auto end = begin + node.child_count;
    if (begin > state.arena.child_refs.size() || end > state.arena.child_refs.size()) {
      preview_visiting[ref] = 0;
      return 0;
    }
    for (std::size_t index = begin; index < end; ++index) {
      const auto found = find_flagged_preview(state.arena.child_refs[index]);
      if (found != 0) {
        preview_visiting[ref] = 0;
        return found;
      }
    }
    preview_visiting[ref] = 0;
    return 0;
  };

  for (NodeRef ref = 0; ref < state.arena.nodes.size(); ++ref) {
    if (cancelled()) return;
    auto& node = state.arena.nodes[ref];
    if (node.kind == NodeKind::media_set) {
      const auto begin = static_cast<std::size_t>(node.first_child);
      const auto end = begin + node.child_count;
      if (begin > state.arena.child_refs.size() || end > state.arena.child_refs.size()) return;
      for (std::size_t index = begin; index < end; ++index) {
        const auto child = state.arena.child_refs[index];
        if (child < state.arena.nodes.size() &&
            state.arena.nodes[child].kind == NodeKind::member &&
            state.arena.nodes[child].media_type == MediaType::image) {
          node.preview_member_id = state.arena.nodes[child].id;
          break;
        }
      }
    } else if (node.kind == NodeKind::work) {
      node.preview_member_id = find_flagged_preview(ref);
    }
  }

  for (auto& cache : state.media_page_cache) cache.clear();
  const auto add_target = [&](NodeRef ref) {
    const auto* node = state.get(ref);
    if (!node) return;
    if (node->kind == NodeKind::member) {
      state.media_page_cache[0].push_back(ref);
      const auto index = static_cast<std::size_t>(node->media_type);
      if (index < 5) state.media_page_cache[index + 1].push_back(ref);
      return;
    }
    if (node->kind != NodeKind::work) return;
    state.media_page_cache[0].push_back(ref);
    for (const auto type : {MediaType::image, MediaType::video, MediaType::audio,
                            MediaType::text, MediaType::document}) {
      if ((node->media_mask & media_type_bit(type)) != 0)
        state.media_page_cache[static_cast<std::size_t>(type) + 1].push_back(ref);
    }
  };

  for (NodeRef ref = 0; ref < state.arena.nodes.size(); ++ref) {
    if (cancelled()) return;
    const auto* node = state.get(ref);
    if (!node || node->kind != NodeKind::work ||
        (node->flags & node_attached_media_flag) != 0) continue;
    std::vector<NodeRef> standalone_members;
    const auto set_begin = static_cast<std::size_t>(node->first_child);
    const auto set_end = set_begin + node->child_count;
    if (set_begin > state.arena.child_refs.size() || set_end > state.arena.child_refs.size()) return;
    for (std::size_t set_index = set_begin; set_index < set_end; ++set_index) {
      const auto set_ref = state.arena.child_refs[set_index];
      const auto* set = state.get(set_ref);
      if (!set || set->kind != NodeKind::media_set) continue;
      const auto member_begin = static_cast<std::size_t>(set->first_child);
      const auto member_end = member_begin + set->child_count;
      if (member_begin > state.arena.child_refs.size() || member_end > state.arena.child_refs.size()) return;
      for (std::size_t member_index = member_begin; member_index < member_end; ++member_index) {
        const auto member_ref = state.arena.child_refs[member_index];
        const auto* member = state.get(member_ref);
        if (member && (member->flags & node_standalone_media_flag) != 0)
          standalone_members.push_back(member_ref);
      }
    }
    if (standalone_members.empty()) add_target(ref);
    else for (const auto member_ref : standalone_members) add_target(member_ref);
  }

  const auto less_recent = [&](NodeRef left, NodeRef right) {
    const auto* lhs = state.get(left);
    const auto* rhs = state.get(right);
    if (!lhs || !rhs) return left < right;
    if (lhs->updated_at != rhs->updated_at) return lhs->updated_at > rhs->updated_at;
    const auto name = ordering::compare_path(state.text(lhs->name), state.text(rhs->name));
    if (name != 0) return name < 0;
    const auto path = ordering::compare_path(state.text(lhs->relative_path), state.text(rhs->relative_path));
    if (path != 0) return path < 0;
    return lhs->id < rhs->id;
  };
  for (auto& cache : state.media_page_cache) std::ranges::sort(cache, less_recent);
}

class GraphBuilder final {
public:
  GraphBuilder(GraphState& state, const std::atomic_bool* stop_requested)
      : state_(state), stop_requested_(stop_requested) {}

  void build(const ScanSnapshot& snapshot) {
    state_.clear();
    state_.diagnostics = snapshot.diagnostics;
    if (cancelled()) return;
    state_.root = build_collection(snapshot.tree, invalid_node, true);
    if (cancelled()) return;
    flush_file_aliases();
    if (cancelled()) return;
    finalize_graph_impl(state_, stop_requested_);
    state_.ready = snapshot.complete && !cancelled() && state_.root != invalid_node;
  }

private:
  bool cancelled() const noexcept {
    return stop_requested_ && stop_requested_->load(std::memory_order_acquire);
  }

  struct PendingFileAlias final {
    ObservedFile file;
    EntryId parent_id = 0;
  };

  EntryId claim_id(EntryId candidate, std::string_view seed) {
    if (candidate == 0) candidate = fnv1a(seed);
    EntryId value = candidate;
    std::size_t collision = 0;
    while (used_ids_.contains(value)) {
      ++collision;
      value = fnv1a(std::string(seed) + "#" + std::to_string(collision));
    }
    used_ids_.insert(value);
    return value;
  }

  EntryId id_for(const ObservedDirectory& directory, NodeKind kind, std::string_view suffix = {}) {
    const auto base = path_text(directory.canonical_relative_path) + ":" +
                      std::to_string(static_cast<int>(kind)) + std::string(suffix);
    return claim_id(directory.persisted_id.value_or(0), base);
  }

  EntryId id_for_file(EntryId parent, const ObservedFile& file, std::string_view suffix = {}) {
    const auto base = std::to_string(parent) + ":" + path_text(file.canonical_relative_path) + std::string(suffix);
    return claim_id(0, base);
  }

  void remember_persisted_id(const ObservedDirectory& directory, NodeRef node) {
    if (directory.persisted_id) persisted_nodes_.emplace(*directory.persisted_id, node);
  }

  void discard_node(const ObservedDirectory& directory, EntryId id, NodeRef node) {
    if (node >= state_.arena.nodes.size()) return;
    // 構築中にこのノードから追加されたNodeは、親へまだ接続されていない。
    // 末尾から一部だけをpopすると、空WorkのMemberやMediaSetが孤立して
    // 検索・content APIから見えてしまうため、追加範囲を一括で巻き戻す。
    std::unordered_set<EntryId> removed_ids;
    for (NodeRef ref = node; ref < state_.arena.nodes.size(); ++ref) {
      const auto* removed = state_.get(ref);
      if (!removed) continue;
      removed_ids.insert(removed->id);
      used_ids_.erase(removed->id);
      state_.tags.erase(removed->id);
    }
    state_.index.erase(id);
    state_.arena.nodes.resize(node);
    std::erase_if(state_.index, [&](const auto& item) {
      return removed_ids.contains(item.first) || removed_ids.contains(item.second.canonical_id) ||
             (!item.second.alias && item.second.node >= node);
    });
    std::erase_if(canonical_nodes_, [&](const auto& item) {
      return removed_ids.contains(state_.get(item.second) ? state_.get(item.second)->id : 0) || item.second >= node;
    });
    std::erase_if(persisted_nodes_, [&](const auto& item) { return item.second >= node; });
    pending_file_aliases_.erase(
        std::remove_if(pending_file_aliases_.begin(), pending_file_aliases_.end(),
                       [&](const auto& pending) { return removed_ids.contains(pending.parent_id); }),
        pending_file_aliases_.end());
    std::erase_if(canonical_files_, [&](const auto& item) { return removed_ids.contains(item.second); });
    std::erase_if(state_.arena.aliases, [&](const auto& alias) {
      if (!removed_ids.contains(alias.parent_id) && !removed_ids.contains(alias.canonical_id)) return false;
      used_ids_.erase(alias.id);
      state_.index.erase(alias.id);
      return true;
    });
    (void)directory;
  }

  Slice store_path(const fs::path& path) { return state_.arena.store(path_text(path)); }

  NodeRef add_node(NodeKind kind, EntryId id, NodeRef parent, MediaType media_type,
                   std::string_view name, const fs::path& relative, const fs::path& canonical,
                   std::int64_t updated_at) {
    NodeRecord node;
    node.id = id;
    node.kind = kind;
    node.media_type = media_type;
    node.media_mask = media_type_bit(media_type);
    node.parent = parent;
    node.name = state_.arena.store(name);
    node.relative_path = store_path(relative);
    node.canonical_relative_path = store_path(canonical);
    node.updated_at = updated_at;
    const NodeRef ref = static_cast<NodeRef>(state_.arena.nodes.size());
    state_.arena.nodes.push_back(node);
    state_.index.emplace(id, Lookup{ref, id, false});
    return ref;
  }

  void add_children(NodeRef parent, const std::vector<NodeRef>& children) {
    auto* node = state_.get(parent);
    if (!node || children.empty()) return;
    node->first_child = static_cast<std::uint32_t>(state_.arena.child_refs.size());
    node->child_count = static_cast<std::uint32_t>(children.size());
    state_.arena.child_refs.insert(state_.arena.child_refs.end(), children.begin(), children.end());
  }

  void add_alias_record(EntryId alias_id, EntryId canonical_id, EntryId parent_id,
                        const fs::path& source_path, const fs::path& canonical_path, bool symlink) {
    AliasRecord alias;
    alias.id = alias_id;
    alias.canonical_id = canonical_id;
    alias.parent_id = parent_id;
    alias.source_path = store_path(source_path);
    alias.canonical_path = store_path(canonical_path);
    alias.symlink = symlink ? 1 : 0;
    state_.arena.aliases.push_back(alias);
    state_.index[alias_id] = Lookup{invalid_node, canonical_id, true};
  }

  void add_alias(const ObservedDirectory& directory, EntryId canonical_id, EntryId parent_id) {
    const EntryId alias_id = claim_id(0, path_text(directory.relative_path) + ":alias");
    add_alias_record(alias_id, canonical_id, parent_id, directory.relative_path,
                     directory.canonical_relative_path, directory.symlink);
  }

  std::string file_key(const ObservedFile& file) const {
    return path_text(file.canonical_relative_path);
  }

  void flush_file_aliases() {
    for (const auto& pending : pending_file_aliases_) {
      if (cancelled()) return;
      const auto found = canonical_files_.find(file_key(pending.file));
      if (found == canonical_files_.end()) {
        state_.diagnostics.push_back({ScanDiagnostic::Level::warning, pending.file.relative_path,
                                      "symlink target member was not built"});
        continue;
      }
      const auto seed = path_text(pending.file.relative_path) + ":file_alias";
      const auto alias_id = claim_id(0, seed);
      add_alias_record(alias_id, found->second, pending.parent_id, pending.file.relative_path,
                       pending.file.canonical_relative_path, pending.file.symlink);
    }
    pending_file_aliases_.clear();
  }

  std::vector<NodeRef> build_media_set(NodeRef work, EntryId work_id, const std::string& label,
                                       const fs::path& base_path, MediaType type,
                                       const std::vector<ObservedFile>& files,
                                       bool standalone = false) {
    std::vector<NodeRef> result;
    if (files.empty()) return result;
    const auto set_seed = std::to_string(work_id) + ":set:" + label + ":" + media_type_name(type);
    const EntryId set_id = claim_id(0, set_seed);
    const NodeRef set_ref = add_node(NodeKind::media_set, set_id, work, type, label, base_path, base_path, 0);
    std::vector<NodeRef> members;
    members.reserve(files.size());
    // file_time_typeのepochは処理系依存で、現在時刻が負値になることもある。
    // 0からmaxを取ると全ファイルの更新時刻が0へ潰れて名前順になるため、
    // 正しい最小値から最大値を集計する。
    std::int64_t updated = std::numeric_limits<std::int64_t>::min();
    for (const auto& file : files) {
      if (cancelled()) return {};
      const auto add_member = [&](std::string_view member_name, MediaType member_type) {
        const auto canonical_key = file_key(file);
        if (file.alias_only || canonical_files_.contains(canonical_key)) {
          pending_file_aliases_.push_back({file, work_id});
          return;
        }
        const EntryId member_id = id_for_file(set_id, file);
        const NodeRef member = add_node(NodeKind::member, member_id, set_ref, member_type,
                                        member_name, file.relative_path, file.canonical_relative_path, file.updated_at);
        auto* member_node = state_.get(member);
        member_node->order = static_cast<std::uint32_t>(members.size());
        if (standalone) member_node->flags |= node_standalone_media_flag;
        member_node->mime_type = state_.arena.store(mime_for(file.name));
        member_node->source_path = state_.arena.store(path_text(file.relative_path));
        members.push_back(member);
        canonical_files_.emplace(canonical_key, member_id);
        updated = std::max(updated, file.updated_at);
      };
      // ZIPは通常の画像メンバー1個として扱う。内部一覧は構築時に読まず、
      // 画像ディレクトリを開いたときにAPIが遅延展開する。
      add_member(file.name, file.media_type);
    }
    if (members.empty()) {
      state_.index.erase(set_id);
      state_.arena.nodes.pop_back();
      return result;
    }
    auto* set_node = state_.get(set_ref);
    set_node->updated_at = updated;
    set_node->media_mask = media_type_bit(type);
    for (const auto member_ref : members) {
      const auto* member = state_.get(member_ref);
      if (member && member->media_type == MediaType::image) {
        set_node->preview_member_id = member->id;
        break;
      }
    }
    add_children(set_ref, members);
    result.push_back(set_ref);
    return result;
  }

  void mark_preview_in_set(NodeRef work, const std::vector<NodeRef>& set_refs) {
    auto* work_node = state_.get(work);
    if (!work_node || work_node->preview_member_id != 0) return;
    for (const auto set_ref : set_refs) {
      if (cancelled()) return;
      const auto* set = state_.get(set_ref);
      if (!set) continue;
      NodeRef preview = invalid_node;
      std::size_t image_count = 0;
      const auto begin = set->first_child;
      const auto end = begin + set->child_count;
      for (std::uint32_t index = begin; index < end && index < state_.arena.child_refs.size(); ++index) {
        if (cancelled()) return;
        const auto member_ref = state_.arena.child_refs[index];
        const auto* member = state_.get(member_ref);
        if (!member || member->media_type != MediaType::image) continue;
        preview = member_ref;
        ++image_count;
      }
      if (image_count != 1 || preview == invalid_node) continue;
      auto* member = state_.get(preview);
      work_node->preview_member_id = member->id;
      member->flags |= node_preview_flag;
      return;
    }
  }

  void mark_first_image_preview(NodeRef work, const std::vector<NodeRef>& set_refs) {
    auto* work_node = state_.get(work);
    if (!work_node || work_node->preview_member_id != 0) return;
    for (const auto set_ref : set_refs) {
      if (cancelled()) return;
      const auto* set = state_.get(set_ref);
      if (!set || set->media_type != MediaType::image) continue;
      const auto begin = set->first_child;
      const auto end = begin + set->child_count;
      for (std::uint32_t index = begin; index < end && index < state_.arena.child_refs.size(); ++index) {
        if (cancelled()) return;
        const auto member_ref = state_.arena.child_refs[index];
        auto* member = state_.get(member_ref);
        if (!member || member->media_type != MediaType::image) continue;
        work_node->preview_member_id = member->id;
        member->flags |= node_preview_flag;
        return;
      }
    }
  }

  void append_media_sets_for_directory(NodeRef work, EntryId work_id,
                                       const ObservedDirectory& directory,
                                       std::vector<NodeRef>& sets,
                                       std::string_view child_label = {}) {
    std::array<std::vector<ObservedFile>, 5> grouped;
    for (const auto& file : directory.files) {
      if (cancelled()) return;
      const auto index = static_cast<std::size_t>(file.media_type);
      if (index < grouped.size()) grouped[index].push_back(file);
    }

    std::size_t direct_media_type_count = 0;
    for (const auto& files : grouped)
      if (!files.empty()) ++direct_media_type_count;

    const bool video_directory = !directory.has_unsupported_files && directory.children.empty() &&
      !grouped[static_cast<std::size_t>(MediaType::video)].empty() &&
      grouped[static_cast<std::size_t>(MediaType::image)].size() <= 1 &&
      grouped[static_cast<std::size_t>(MediaType::audio)].empty() &&
      grouped[static_cast<std::size_t>(MediaType::text)].empty() &&
      grouped[static_cast<std::size_t>(MediaType::document)].empty();

    if (!video_directory && direct_media_type_count > 1) {
      // 画像・動画・音声などが同じディレクトリに直接置かれた場合は、
      // 種別をまとめて一つのSetにせず、旧mixed directoryと同じく各ファイルを
      // 独立したMediaとして扱う。未知形式のファイルは従来どおり対象外だが、
      // 認識できるファイル同士の混在によって全体を破棄してはならない。
      for (const auto& file : directory.files) {
        if (cancelled()) return;
        std::vector<ObservedFile> single_file{file};
        auto file_sets = build_media_set(work, work_id, file.name, directory.relative_path,
                                         file.media_type, single_file, true);
        sets.insert(sets.end(), file_sets.begin(), file_sets.end());
      }
      return;
    }

    if (!video_directory && !grouped[static_cast<std::size_t>(MediaType::video)].empty()) {
      state_.diagnostics.push_back({ScanDiagnostic::Level::warning, directory.relative_path,
                                    "video files ignored outside the video-directory format"});
      grouped[static_cast<std::size_t>(MediaType::video)].clear();
    }

    if (video_directory) {
      for (std::size_t index = 0; index < grouped.size(); ++index) {
        if (cancelled()) return;
        const auto type = static_cast<MediaType>(index);
        if (type == MediaType::image || type == MediaType::video) continue;
        const auto label = child_label.empty() ? std::string(media_type_name(type)) : std::string(child_label);
        auto child_sets = build_media_set(work, work_id, label, directory.relative_path, type, grouped[index]);
        sets.insert(sets.end(), child_sets.begin(), child_sets.end());
      }
      auto video_files = grouped[static_cast<std::size_t>(MediaType::video)];
      video_files.insert(video_files.end(), grouped[static_cast<std::size_t>(MediaType::image)].begin(),
                         grouped[static_cast<std::size_t>(MediaType::image)].end());
      const auto label = child_label.empty() ? std::string(media_type_name(MediaType::video)) : std::string(child_label);
      auto video_sets = build_media_set(work, work_id, label, directory.relative_path, MediaType::video, video_files);
      mark_preview_in_set(work, video_sets);
      sets.insert(sets.end(), video_sets.begin(), video_sets.end());
      return;
    }

    for (std::size_t index = 0; index < grouped.size(); ++index) {
      if (cancelled()) return;
      const auto type = static_cast<MediaType>(index);
      const auto label = child_label.empty() ? std::string(media_type_name(type)) : std::string(child_label);
      auto child_sets = build_media_set(work, work_id, label, directory.relative_path, type, grouped[index]);
      sets.insert(sets.end(), child_sets.begin(), child_sets.end());
    }
  }

  NodeRef build_attached_media_work(NodeRef collection, const ObservedDirectory& directory,
                                    const ObservedFile& file) {
    if (cancelled()) return invalid_node;
    if (file.alias_only) {
      pending_file_aliases_.push_back({file, state_.get(collection)->id});
      return invalid_node;
    }

    const auto seed = std::string("attached-work:") + path_text(file.canonical_relative_path);
    const EntryId id = claim_id(0, seed);
    const NodeRef work = add_node(NodeKind::work, id, collection, MediaType::unknown,
                                  file.name, file.relative_path, file.canonical_relative_path,
                                  file.updated_at);
    state_.tags[id] = {};
    if (auto* work_node = state_.get(work)) work_node->flags |= node_attached_media_flag;

    const std::vector<ObservedFile> members{file};
    auto sets = build_media_set(work, id, file.name, directory.relative_path,
                                file.media_type, members);
    if (sets.empty()) {
      discard_node(directory, id, work);
      return invalid_node;
    }
    add_children(work, sets);
    if (file.media_type == MediaType::image) mark_first_image_preview(work, sets);
    if (auto* work_node = state_.get(work)) {
      for (const auto set_ref : sets) {
        if (const auto* set = state_.get(set_ref)) {
          work_node->updated_at = std::max(work_node->updated_at, set->updated_at);
          work_node->media_mask |= set->media_mask;
        }
      }
    }
    return work;
  }

  NodeRef build_work(const ObservedDirectory& directory, NodeRef parent) {
    const auto key = directory.canonical_relative_path.generic_string();
    if (const auto found = canonical_nodes_.find(key); found != canonical_nodes_.end()) {
      add_alias(directory, state_.get(found->second)->id, state_.get(parent)->id);
      return invalid_node;
    }
    if (directory.persisted_id) {
      if (const auto found = persisted_nodes_.find(*directory.persisted_id); found != persisted_nodes_.end()) {
        add_alias(directory, state_.get(found->second)->id, state_.get(parent)->id);
        CROW_LOG_WARNING << "viewer duplicate logical id: " << directory.relative_path.generic_string();
        return invalid_node;
      }
    }

    const EntryId id = id_for(directory, NodeKind::work);
    const NodeRef work = add_node(NodeKind::work, id, parent, MediaType::unknown,
                                  directory.name, directory.relative_path, directory.canonical_relative_path,
                                  directory.updated_at);
    canonical_nodes_[key] = work;
    state_.tags[id] = directory.tags;
    for (const auto& tag : directory.legacy_info_tags)
      if (std::ranges::find(state_.tags[id], tag) == state_.tags[id].end()) state_.tags[id].push_back(tag);
    remember_persisted_id(directory, work);

    std::vector<NodeRef> sets;
    // 直下の独立メディアは、動画葉の有無にかかわらず保持する。動画葉を
    // 束ねる形式で直下動画が混在する場合も、append_media_sets_for_directory
    // が独立MediaSetとして扱う既存契約を維持する。
    append_media_sets_for_directory(work, id, directory, sets);

    const auto append_directory_sets = [&](const ObservedDirectory& child) -> void {
      if (cancelled()) return;
      if (child.alias_only) {
        const auto found = canonical_nodes_.find(child.canonical_relative_path.generic_string());
        if (found != canonical_nodes_.end()) add_alias(child, state_.get(found->second)->id, id);
        return;
      }
      append_media_sets_for_directory(work, id, child, sets, child.name);
      // WorkのMediaSetはWork自身、またはWork直下のメディア葉からだけ作る。
      // 孫以下まで再帰すると、途中のディレクトリをグラフから飛ばして
      // 下位のメディアだけがMediaSetsに現れ、ディレクトリ構造が一段深く
      // 表示される。葉ノード単位の契約に合わない構造は隠蔽せず警告する。
      if (std::ranges::any_of(child.children, has_media_in_subtree)) {
        state_.diagnostics.push_back({ScanDiagnostic::Level::warning, child.relative_path,
                                      "nested media directory is ignored; media directories must be direct Work children"});
      }
    };
    for (const auto& child : directory.children) {
      if (cancelled()) return invalid_node;
      append_directory_sets(child);
    }
    if (sets.empty()) {
      discard_node(directory, id, work);
      return invalid_node;
    }
    add_children(work, sets);
    // Work全体のサムネイルは、旧実装と同じく直接画像を持つ画像ディレクトリ
    // に限る。音声・テキスト・混合ディレクトリや、子孫の画像を集約しただけの
    // コンテナには画像を貼らず、通常のボタンとして表示する。
    if (is_direct_image_directory(directory)) mark_first_image_preview(work, sets);
    if (auto* work_node = state_.get(work)) {
      for (const auto set_ref : sets) {
        if (const auto* set = state_.get(set_ref)) {
          work_node->updated_at = std::max(work_node->updated_at, set->updated_at);
          work_node->media_mask |= set->media_mask;
        }
      }
    }

    return work;
  }

  NodeRef build_collection(const ObservedDirectory& directory, NodeRef parent, bool root) {
    const auto key = directory.canonical_relative_path.generic_string();
    if (!root) {
      if (const auto found = canonical_nodes_.find(key); found != canonical_nodes_.end()) {
        add_alias(directory, state_.get(found->second)->id, state_.get(parent)->id);
        return invalid_node;
      }
      if (directory.persisted_id) {
        if (const auto found = persisted_nodes_.find(*directory.persisted_id); found != persisted_nodes_.end()) {
          add_alias(directory, state_.get(found->second)->id, state_.get(parent)->id);
          CROW_LOG_WARNING << "viewer duplicate logical id: " << directory.relative_path.generic_string();
          return invalid_node;
        }
      }
    }

    const EntryId id = id_for(directory, NodeKind::collection);
    const NodeRef collection = add_node(NodeKind::collection, id, parent, MediaType::unknown,
                                        directory.name, directory.relative_path, directory.canonical_relative_path,
                                        directory.updated_at);
    canonical_nodes_[key] = collection;
    state_.tags[id] = directory.tags;
    for (const auto& tag : directory.legacy_info_tags)
      if (std::ranges::find(state_.tags[id], tag) == state_.tags[id].end()) state_.tags[id].push_back(tag);
    remember_persisted_id(directory, collection);

    std::vector<NodeRef> children;
    for (const auto& file : directory.files) {
      const auto attached = build_attached_media_work(collection, directory, file);
      if (attached != invalid_node) children.push_back(attached);
    }
    for (const auto& child : directory.children) {
      if (cancelled()) return invalid_node;
      if (child.alias_only) {
        const auto found = canonical_nodes_.find(child.canonical_relative_path.generic_string());
        if (found != canonical_nodes_.end()) add_alias(child, state_.get(found->second)->id, id);
        continue;
      }
      if (is_work_directory(child)) {
        const auto work = build_work(child, collection);
        if (work != invalid_node) children.push_back(work);
      } else {
        if (!child.children.empty()) {
          const auto nested = build_collection(child, collection, false);
          if (nested != invalid_node) children.push_back(nested);
        }
      }
    }
    if (!root && children.empty()) {
      discard_node(directory, id, collection);
      return invalid_node;
    }
    add_children(collection, children);
    if (auto* collection_node = state_.get(collection)) {
      for (const auto child_ref : children)
        if (const auto* child = state_.get(child_ref)) {
          collection_node->updated_at = std::max(collection_node->updated_at, child->updated_at);
          collection_node->media_mask |= child->media_mask;
        }
    }
    return collection;
  }

  GraphState& state_;
  std::set<EntryId> used_ids_;
  std::unordered_map<std::string, NodeRef> canonical_nodes_;
  std::unordered_map<EntryId, NodeRef> persisted_nodes_;
  std::unordered_map<std::string, EntryId> canonical_files_;
  std::vector<PendingFileAlias> pending_file_aliases_;
  const std::atomic_bool* stop_requested_ = nullptr;
};

} // namespace

void build_graph(GraphState& state, const std::filesystem::path& root, const ScanSnapshot& snapshot,
                 const std::atomic_bool* stop_requested) {
  (void)root;
  GraphBuilder builder(state, stop_requested);
  builder.build(snapshot);
}

} // namespace VIEWER
