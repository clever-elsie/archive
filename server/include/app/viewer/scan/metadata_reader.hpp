#pragma once

#include <filesystem>
#include <vector>

#include <app/viewer/model/observation.hpp>

namespace VIEWER::scan {

void load_metadata(const std::filesystem::path& directory, ObservedDirectory& out,
                   std::vector<ScanDiagnostic>& diagnostics);

} // namespace VIEWER::scan
