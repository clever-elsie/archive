#pragma once
#include <string>
#include <cstdint>

namespace utf8{ // UTF-8 ユーティリティ
using namespace std;
uint32_t decode_one(const char*& it, const char* end);
void encode_one(uint32_t cp, string& out);
uint32_t normalize_hiragana_base(uint32_t cp); // 拗音（小書き）・濁音・半濁音を基底のひらがなへ正規化

inline constexpr bool ishiragana(uint32_t cp){
  return 0x3041 <= cp && cp <= 0x3096;
}
inline constexpr bool iskatakana(uint32_t cp){
  return 0x30A1 <= cp && cp <= 0x30FA;
}
inline constexpr bool islower(uint32_t cp){ return (cp >= 'a' && cp <= 'z'); }
inline constexpr bool isupper(uint32_t cp){ return (cp >= 'A' && cp <= 'Z'); }
inline constexpr bool isalpha(uint32_t cp){ return islower(cp) || isupper(cp); }
inline constexpr bool isdigit(uint32_t cp){ return (cp >= '0' && cp <= '9'); }

inline constexpr uint32_t tohiragana(uint32_t cp) {
  if (iskatakana(cp))  // おおよそ対応（拗音・濁点含む基本ブロック）
    return cp - 0x60;  // カタカナ→ひらがな
  return cp;
}

inline constexpr uint32_t tolower(uint32_t cp) {
  if (isupper(cp)) return cp + ('a' - 'A');
  return cp;
}
inline constexpr uint32_t toupper(uint32_t cp) {
  if (islower(cp)) return cp - ('a' - 'A');
  return cp;
}

} // namespace utf8