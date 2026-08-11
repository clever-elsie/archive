#include <app/viewer/manager.hpp>

#include <app/viewer/access/acl.hpp>

namespace VIEWER {

bool Manager::can_access_path(std::string_view target) const {
  return access::can_access_path(access_rules_, target);
}

bool Manager::can_access(const GraphState& state, NodeRef node_ref, bool administrator) const {
  if (administrator) return true;
  const auto* node = state.get(node_ref);
  if (!node) return false;
  const auto path = state.text(node->relative_path);
  if (!can_access_path(path)) return false;
  const auto canonical = state.text(node->canonical_relative_path);
  return canonical == path || canonical.empty() || can_access_path(canonical);
}

bool Manager::can_access_alias(const GraphState& state, const AliasRecord& alias, bool administrator) const {
  if (administrator) return true;
  return can_access_path(state.text(alias.source_path)) && can_access_path(state.text(alias.canonical_path));
}

} // namespace VIEWER
