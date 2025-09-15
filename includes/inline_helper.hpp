#pragma once
#include <string>

// HTMLエスケープ関数
inline std::string html_escape(const std::string& input) {
  std::string out;
  out.reserve(input.size());
  for (char c : input) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c; break;
    }
  }
  return out;
}