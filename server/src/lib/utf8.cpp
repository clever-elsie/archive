#include <bit>

#include <lib/utf8.hpp>

namespace utf8{
using namespace std;

uint32_t decode_one(const char*& it, const char* end){
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

void encode_one(uint32_t cp, string& out) {
  if (cp <= 0x7F) { out.push_back(static_cast<char>(cp)); return; }
  int len = (cp <= 0x7FF) ? 2 : (cp <= 0xFFFF) ? 3 : 4;
  char buf[4];
  for (int i = len - 1; i > 0; --i) {
    buf[i] = static_cast<char>(0x80 | (cp & 0x3F));
    cp >>= 6;
  }
  static const uint8_t first_mask[5] = {0, 0, 0xC0, 0xE0, 0xF0};
  buf[0] = static_cast<char>(first_mask[len] | (cp & (0xFFu >> (len + 1))));
  out.append(buf, len);
}

uint32_t normalize_hiragana_base(uint32_t cp){
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
    case 0x3094: return 0x3046; // ゔ→う
    case 0x3095: return 0x304B; // ゕ→か
    case 0x3096: return 0x3051; // ゖ→け
    default: return cp;
  }
}
} // namespace utf8