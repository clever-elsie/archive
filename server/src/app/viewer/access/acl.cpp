#include <app/viewer/access/acl.hpp>

#include <filesystem>

namespace VIEWER::access {
namespace {

std::string normalized_rule(std::string_view raw, bool& deny) {
  deny = !raw.empty() && raw.front() == '!';
  if (deny) raw.remove_prefix(1);
  if (raw.empty()) return {};
  const std::filesystem::path original(raw);
  if (original.is_absolute()) return {};
  const auto normalized = original.lexically_normal();
  if (normalized == "." || normalized.empty()) return {};
  for (const auto& part : normalized)
    if (part == "..") return {};
  return normalized.generic_string();
}

bool is_prefix(std::string_view prefix, std::string_view value) {
  return value == prefix || (value.size() > prefix.size() && value.starts_with(prefix) && value[prefix.size()] == '/');
}

std::vector<std::string> path_components(std::string_view path) {
  std::vector<std::string> result;
  std::size_t start = 0;
  while (start < path.size()) {
    const auto slash = path.find('/', start);
    const auto end = slash == std::string_view::npos ? path.size() : slash;
    if (end > start) result.emplace_back(path.substr(start, end - start));
    if (slash == std::string_view::npos) break;
    start = slash + 1;
  }
  return result;
}

} // namespace

bool can_access_path(const std::vector<std::string>& rules, std::string_view target) {
  if (target.empty()) return true;
  const auto parts = path_components(target);
  std::string current;
  for (std::size_t index = 0; index < parts.size(); ++index) {
    if (!current.empty()) current.push_back('/');
    current += parts[index];
    bool decision = false;
    bool event = false;
    for (const auto& raw : rules) {
      bool deny = false;
      const auto rule = normalized_rule(raw, deny);
      if (rule.empty()) continue;
      if (is_prefix(rule, current)) {
        decision = !deny;
        event = true;
      } else if (!deny && is_prefix(current, rule) &&
                 (current == target || is_prefix(rule, target))) {
        decision = true;
        event = true;
      }
    }
    if (!event || !decision) return false;
  }
  return true;
}

} // namespace VIEWER::access
