#pragma once

#include <atomic>
#include <filesystem>

#include <app/viewer/model/graph.hpp>
#include <app/viewer/model/observation.hpp>

namespace VIEWER {

void build_graph(GraphState& state, const std::filesystem::path& root, const ScanSnapshot& snapshot,
                 const std::atomic_bool* stop_requested = nullptr);

} // namespace VIEWER
