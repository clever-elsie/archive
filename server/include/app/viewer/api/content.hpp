#pragma once

#include <string>
#include <string_view>

namespace VIEWER::api {

std::string encode_path(std::string_view value);

} // namespace VIEWER::api
