#pragma once

#include <filesystem>

namespace VIEWER::content {

bool path_within(const std::filesystem::path& root, const std::filesystem::path& candidate);

} // namespace VIEWER::content
