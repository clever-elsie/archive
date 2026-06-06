#pragma once
#include <string>
#include <cstdint>

namespace utf8{ // UTF-8 ユーティリティ
using namespace std;
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
  if (iskatakana(cp)){ // おおよそ対応（拗音・濁点含む基本ブロック）
    if(0x30f7<=cp) cp = 0x30ef + cp-0x30f7; // 濁点付きワヰヱヲはひらがなにないから濁点を消す
    return cp - 0x60;  // カタカナ→ひらがな
  }
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

inline constexpr uint32_t decode_one(const char*& it, const char* end){
  if(it>=end) return 0;
  uint8_t c = uint8_t(*it++);
  if(c < 0x80) return c;
  const int len=std::countl_one(c);
  if(len<2||len>4||end-it<len-1) return 0;
  uint32_t cp=c&(0xFFu>>len);
  for(int i=1;i<len;++i){
    uint8_t t=(uint8_t)*it++;
    if((t&0xC0)!=0x80) return 0;
    cp=(cp<<6)|(t&0x3F);
  }
  return cp;
}

inline constexpr void encode_one(uint32_t cp, string& out) {
  if (cp <= 0x7F) { out.push_back(static_cast<char>(cp)); return; }
  int len = (cp <= 0x7FF) ? 2 : (cp <= 0xFFFF) ? 3 : 4;
  char buf[4];
  for (int i = len - 1; i > 0; --i) {
    buf[i] = static_cast<char>(0x80 | (cp & 0x3F));
    cp >>= 6;
  }
  if consteval{
      const uint8_t first_mask[5] = {0, 0, 0xC0, 0xE0, 0xF0};
      buf[0] = static_cast<char>(first_mask[len] | (cp & (0xFFu >> (len + 1))));
      out.append(buf, len);
  }else{
      static const uint8_t first_mask[5] = {0, 0, 0xC0, 0xE0, 0xF0};
      buf[0] = static_cast<char>(first_mask[len] | (cp & (0xFFu >> (len + 1))));
      out.append(buf, len);
  }
}

inline constexpr uint32_t normalize_hiragana_base(uint32_t cp){
  switch(cp){
    case 0x3041 ... 0x3049: // ぁ→あ, ぃ→い, ぅ→う, ぇ→え, ぉ→お
      return cp + (cp&1);
    case 0x304c ... 0x3062: // が行 ざ行 だ，ぢ
      return cp - !(cp&1); // 偶数のとき一つ前の奇数に戻す
    case 0x3063: // っ→つ
      return 0x3064;
    case 0x3064 ... 0x3069: // づ，で，ど
      return cp - (cp&1);
    case 0x306F ... 0x307D: // は行 ば行 ぱ行
      return cp - cp%3;
    case 0x3083 ... 0x3087: // ゃ→や, ゅ→ゆ, ょ→よ, ゎ→わ
      return cp + (cp&1);
    case 0x308E: return 0x308F; // ゎ→わ
    case 0x3090: return 0x3044; // ゐ-い
    case 0x3091: return 0x3048; // ゑ-え
    case 0x3094: return 0x3046; // ゔ→う
    case 0x3095: return 0x304B; // ゕ→か
    case 0x3096: return 0x3051; // ゖ→け
    default: return cp;
  }
}

} // namespace utf8
