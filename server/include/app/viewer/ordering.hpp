#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace VIEWER::ordering {

struct DisplayName final {
  std::string base;
  std::string ruby;
};

// ルビ・UTF-8正規化・自然数比較を共有する比較器。
std::string sort_key(std::string_view path_element);
DisplayName display_name(std::string_view path_element);
int compare_component(std::string_view left, std::string_view right);
int compare_path(std::string_view left, std::string_view right);
bool less_path(const std::filesystem::path& left, const std::filesystem::path& right);

} // namespace VIEWER::ordering
