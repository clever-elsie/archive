#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <app/viewer/model/graph.hpp>

namespace VIEWER::metadata {

bool write_tags(const std::filesystem::path& root, const NodeRecord& node,
                const GraphState& state, const std::vector<std::string>& tags);

} // namespace VIEWER::metadata
