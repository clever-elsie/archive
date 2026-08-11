#include <app/viewer/manager.hpp>

#include <system_error>

#include <app/viewer/content/filesystem_guard.hpp>

namespace VIEWER {

bool Manager::source_available(std::string_view relative_path) const {
  const std::filesystem::path relative(relative_path);
  if (relative.is_absolute() || relative.lexically_normal() != relative) return false;
  std::error_code ec;
  const auto root = std::filesystem::weakly_canonical(root_, ec);
  if (ec) return false;
  const auto source = std::filesystem::weakly_canonical(root_ / relative, ec);
  if (ec || !content::path_within(root, source)) return false;
  return std::filesystem::is_regular_file(source, ec) && !ec;
}

bool Manager::source_available(const GraphState& state, NodeRef node_ref) const {
  const auto* node = state.get(node_ref);
  if (!node) return false;
  const auto source_path = state.text(node->source_path);
  const auto canonical_path = state.text(node->canonical_relative_path);
  if (!source_available(source_path) || canonical_path.empty()) return false;
  std::error_code ec;
  const auto root = std::filesystem::weakly_canonical(root_, ec);
  if (ec) return false;
  const auto actual = std::filesystem::weakly_canonical(root_ / std::filesystem::path(source_path), ec);
  if (ec) return false;
  const auto expected = std::filesystem::weakly_canonical(root_ / std::filesystem::path(canonical_path), ec);
  if (ec || !content::path_within(root, expected)) return false;
  return actual == expected;
}

} // namespace VIEWER
