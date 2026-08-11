#include <app/viewer/api/content.hpp>

#include <cctype>

namespace VIEWER::api {

std::string encode_path(std::string_view value) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  for (const unsigned char c : value) {
    if (std::isalnum(c) || c == '/' || c == '-' || c == '_' || c == '.' || c == '~') {
      result.push_back(static_cast<char>(c));
    } else {
      result.push_back('%');
      result.push_back(hex[c >> 4]);
      result.push_back(hex[c & 0x0f]);
    }
  }
  return result;
}

} // namespace VIEWER::api
