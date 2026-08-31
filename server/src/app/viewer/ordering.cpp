#include <app/viewer/ordering.hpp>

#include <algorithm>
#include <cctype>
#include <vector>

#include <lib/utf8.hpp>

namespace VIEWER::ordering {
namespace {

uint32_t get_vowel_hiragana(uint32_t cp) {
  if (utf8::iskatakana(cp)) cp = utf8::tohiragana(cp);
  if (utf8::ishiragana(cp)) cp = utf8::normalize_hiragana_base(cp);

  switch (cp) {
    case 0x3041: case 0x3042:
    case 0x304b: case 0x304c:
    case 0x3055: case 0x3056:
    case 0x305f: case 0x3060:
    case 0x306a:
    case 0x306f: case 0x3070: case 0x3071:
    case 0x307e:
    case 0x3083: case 0x3084:
    case 0x3089:
    case 0x308e: case 0x308f:
    case 0x3095:
      return 0x3042; // あ

    case 0x3043: case 0x3044:
    case 0x304d: case 0x304e:
    case 0x3057: case 0x3058:
    case 0x3061: case 0x3062:
    case 0x306b:
    case 0x3072: case 0x3073: case 0x3074:
    case 0x307f:
    case 0x308a:
    case 0x3090:
      return 0x3044; // い

    case 0x3045: case 0x3046:
    case 0x304f: case 0x3050:
    case 0x3059: case 0x305a:
    case 0x3063: case 0x3064: case 0x3065:
    case 0x306c:
    case 0x3075: case 0x3076: case 0x3077:
    case 0x3080:
    case 0x3085: case 0x3086:
    case 0x308b:
    case 0x3094:
      return 0x3046; // う

    case 0x3047: case 0x3048:
    case 0x3051: case 0x3052:
    case 0x305b: case 0x305c:
    case 0x3066: case 0x3067:
    case 0x306d:
    case 0x3078: case 0x3079: case 0x307a:
    case 0x3081:
    case 0x308c:
    case 0x3091:
    case 0x3096:
      return 0x3048; // え

    case 0x3049: case 0x304a:
    case 0x3053: case 0x3054:
    case 0x305d: case 0x305e:
    case 0x3068: case 0x3069:
    case 0x306e:
    case 0x307b: case 0x307c: case 0x307d:
    case 0x3082:
    case 0x3087: case 0x3088:
    case 0x308d:
    case 0x3092:
      return 0x304a; // お

    default:
      return 0;
  }
}

std::string normalized_utf8(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  const char* it = value.data();
  const char* end = value.data() + value.size();
  std::uint32_t last_vowel = 0;
  while (it < end) {
    const char* begin = it;
    const auto codepoint = utf8::decode_one(it, end);
    if (codepoint == 0) {
      result.append(begin, end);
      break;
    }
    auto normalized = utf8::tolower(codepoint);
    if (utf8::iskatakana(normalized)) normalized = utf8::tohiragana(normalized);
    if (normalized == 0x30FC) {
      if (last_vowel != 0) {
        normalized = last_vowel;
      }
    } else {
      if (utf8::ishiragana(normalized)) normalized = utf8::normalize_hiragana_base(normalized);
      const auto vowel = get_vowel_hiragana(normalized);
      if (vowel != 0) {
        last_vowel = vowel;
      }
    }
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
                           !utf8::ishiragana(codepoint) && !utf8::iskatakana(codepoint) &&
                           codepoint != 0x30FC))
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
