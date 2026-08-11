#include <app/viewer/ordering.hpp>

#include <algorithm>
#include <cctype>
#include <vector>

#include <lib/utf8.hpp>

namespace VIEWER::ordering {
namespace {

std::string normalized_utf8(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  const char* it = value.data();
  const char* end = value.data() + value.size();
  while (it < end) {
    const char* begin = it;
    const auto codepoint = utf8::decode_one(it, end);
    if (codepoint == 0) {
      result.append(begin, end);
      break;
    }
    auto normalized = utf8::tolower(codepoint);
    if (utf8::iskatakana(normalized)) normalized = utf8::tohiragana(normalized);
    if (utf8::ishiragana(normalized)) normalized = utf8::normalize_hiragana_base(normalized);
    utf8::encode_one(normalized, result);
  }
  return result;
}

bool valid_ruby(std::string_view value) {
  const char* it = value.data();
  const char* end = value.data() + value.size();
  while (it < end) {
    const auto codepoint = utf8::decode_one(it, end);
    if (codepoint == 0 || (!utf8::isalpha(codepoint) && !utf8::isdigit(codepoint) &&
                           !utf8::ishiragana(codepoint) && !utf8::iskatakana(codepoint)))
      return false;
  }
  return !value.empty();
}

std::string component_key(std::string_view value) {
  const auto begin = value.rfind("《");
  const auto end = value.rfind("》");
  if (begin != std::string_view::npos && end != std::string_view::npos && begin < end) {
    const auto inner = value.substr(begin + std::string_view("《").size(),
                                    end - begin - std::string_view("《").size());
    if (valid_ruby(inner)) return normalized_utf8(inner);
  }
  return normalized_utf8(value);
}

DisplayName split_display_name(std::string_view value) {
  DisplayName result{std::string(value), {}};
  const auto begin = value.rfind("《");
  const auto end = value.rfind("》");
  if (begin == std::string_view::npos || end == std::string_view::npos || begin >= end)
    return result;
  const auto inner = value.substr(begin + std::string_view("《").size(),
                                  end - begin - std::string_view("《").size());
  if (!valid_ruby(inner)) return result;
  result.base = std::string(value.substr(0, begin));
  result.base += std::string(value.substr(end + std::string_view("》").size()));
  result.ruby = std::string(inner);
  return result;
}

int natural_compare(std::string_view left, std::string_view right) {
  std::size_t il = 0;
  std::size_t ir = 0;
  while (il < left.size() && ir < right.size()) {
    const bool left_digit = std::isdigit(static_cast<unsigned char>(left[il])) != 0;
    const bool right_digit = std::isdigit(static_cast<unsigned char>(right[ir])) != 0;
    if (left_digit && right_digit) {
      std::size_t el = il;
      std::size_t er = ir;
      while (el < left.size() && std::isdigit(static_cast<unsigned char>(left[el]))) ++el;
      while (er < right.size() && std::isdigit(static_cast<unsigned char>(right[er]))) ++er;
      const auto ln = left.substr(il, el - il);
      const auto rn = right.substr(ir, er - ir);
      const auto lfirst = ln.find_first_not_of('0');
      const auto rfirst = rn.find_first_not_of('0');
      const auto lvalue = lfirst == std::string_view::npos ? std::string_view("0") : ln.substr(lfirst);
      const auto rvalue = rfirst == std::string_view::npos ? std::string_view("0") : rn.substr(rfirst);
      if (lvalue.size() != rvalue.size()) return lvalue.size() < rvalue.size() ? -1 : 1;
      if (lvalue != rvalue) return lvalue < rvalue ? -1 : 1;
      if (ln.size() != rn.size()) return ln.size() < rn.size() ? 1 : -1;
      il = el;
      ir = er;
      continue;
    }
    const auto lc = static_cast<unsigned char>(left[il]);
    const auto rc = static_cast<unsigned char>(right[ir]);
    if (lc != rc) return lc < rc ? -1 : 1;
    ++il;
    ++ir;
  }
  if (il != left.size() || ir != right.size()) return il == left.size() ? -1 : 1;
  return 0;
}

std::vector<std::string_view> split_path(std::string_view value) {
  std::vector<std::string_view> result;
  std::size_t begin = 0;
  while (begin <= value.size()) {
    const auto slash = value.find('/', begin);
    const auto end = slash == std::string_view::npos ? value.size() : slash;
    result.push_back(value.substr(begin, end - begin));
    if (slash == std::string_view::npos) break;
    begin = slash + 1;
  }
  return result;
}

} // namespace

std::string sort_key(std::string_view path_element) {
  return component_key(path_element);
}

DisplayName display_name(std::string_view path_element) {
  return split_display_name(path_element);
}

int compare_component(std::string_view left, std::string_view right) {
  const auto left_key = component_key(left);
  const auto right_key = component_key(right);
  const auto result = natural_compare(left_key, right_key);
  if (result != 0) return result;
  if (left == right) return 0;
  return left < right ? -1 : 1;
}

int compare_path(std::string_view left, std::string_view right) {
  const auto left_parts = split_path(left);
  const auto right_parts = split_path(right);
  const auto common = std::min(left_parts.size(), right_parts.size());
  for (std::size_t index = 0; index < common; ++index) {
    const auto result = compare_component(left_parts[index], right_parts[index]);
    if (result != 0) return result;
  }
  if (left_parts.size() != right_parts.size()) return left_parts.size() < right_parts.size() ? -1 : 1;
  if (left == right) return 0;
  return left < right ? -1 : 1;
}

bool less_path(const std::filesystem::path& left, const std::filesystem::path& right) {
  return compare_path(left.generic_string(), right.generic_string()) < 0;
}

} // namespace VIEWER::ordering
