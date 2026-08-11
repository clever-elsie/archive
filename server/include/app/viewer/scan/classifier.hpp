#pragma once

#include <filesystem>

#include <app/viewer/model/types.hpp>

namespace VIEWER::scan {

MediaType classify(const std::filesystem::path& path, bool& archive);
bool is_metadata_file(const std::filesystem::path& path);

} // namespace VIEWER::scan
