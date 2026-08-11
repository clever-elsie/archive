#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace VIEWER::access {

bool can_access_path(const std::vector<std::string>& rules, std::string_view target);

} // namespace VIEWER::access
