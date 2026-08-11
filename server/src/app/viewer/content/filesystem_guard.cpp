#include <app/viewer/content/filesystem_guard.hpp>

namespace VIEWER::content {

bool path_within(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  auto root_it = root.begin();
  auto candidate_it = candidate.begin();
  for (; root_it != root.end() && candidate_it != candidate.end(); ++root_it, ++candidate_it)
    if (*root_it != *candidate_it) return false;
  return root_it == root.end();
}

} // namespace VIEWER::content
