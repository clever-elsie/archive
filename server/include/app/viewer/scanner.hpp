#pragma once

#include <atomic>
#include <filesystem>

#include <app/viewer/model/observation.hpp>

namespace VIEWER {

class Scanner final {
public:
  static ScanSnapshot scan(const std::filesystem::path& root,
                           const std::atomic_bool* stop_requested = nullptr);
};

} // namespace VIEWER
