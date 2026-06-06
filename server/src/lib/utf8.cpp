#include <bit>
#include <format>
#include <string_view>

#include <lib/utf8.hpp>

namespace utf8::test{

struct test_result{
    bool success;
    struct msg_t{
        char buffer[512];
        constexpr const char* data() const { return buffer; }
        constexpr size_t size() const{
            size_t n=0;
            while(n<512 && buffer[n]) ++n;
            return n;
        }
    }message;
};

constexpr void append_str(char*buffer, size_t&idx, std::string_view s){
    for(char c:s) if(idx<511) buffer[idx++] = c;
}

consteval static
std::vector<uint32_t> to_unicode(const std::string_view&s){
    const char*it=s.data();
    const char*end = s.data()+s.size();
    std::vector<uint32_t> ret;
    while(it<end){
        const uint32_t cp = decode_one(it, end);
        if(cp==0) return{};
        ret.push_back(cp);
    }
    return ret;
}

consteval static // katakana->hiragana, hiragana->clear hiragana
std::vector<uint32_t> to_normalized_unicode(const std::string_view&s){
    auto v = to_unicode(s);
    for(auto&c:v) c=normalize_hiragana_base(tohiragana(c));
    return v;
}

consteval static
std::string to_string(const std::vector<uint32_t>&v){
    std::string ret;
    ret.reserve(v.size());
    for(const auto&x:v)
        encode_one(x,ret);
    return ret;
}

consteval static
bool same(const std::vector<uint32_t>&a, const std::vector<uint32_t>&b){
    if(a.size() != b.size()) return false;
    for(size_t i=0;i<a.size();++i)
        if(a[i] != b[i]) return false;
    return true;
}

consteval static
test_result test(){
    using std::string_view;
    using psv = std::pair<string_view, string_view>;
    constexpr psv test_answer[]={
        {"ぁぃぅぇぉ"  ,"あいうえお"},
        {"がぎぐげご"  ,"かきくけこ"},
        {"ざじずぜぞ"  ,"さしすせそ"},
        {"だぢっづでど","たちつつてと"},
        {"ばびぶべぼ"  ,"はひふへほ"},
        {"ぱぴぷぺぽ"  ,"はひふへほ"},
        {"ゃゅょ"      ,"やゆよ"},
        {"ゎゐゑゔゕゖ","わいえうかけ"},
        {"ァィゥェォ"  ,"あいうえお"},
        {"ガギグゲゴ"  ,"かきくけこ"},
        {"ザジズゼゾ"  ,"さしすせそ"},
        {"ダヂッヅデド","たちつつてと"},
        {"バビブベボ"  ,"はひふへほ"},
        {"パピプペポ"  ,"はひふへほ"},
        {"ャュョ"      ,"やゆよ"},
        {"ヮヰヱヴヵヶヷヸヹヺ","わいえうかけわいえを"},
    };
    for(const auto&[tar,ans]:test_answer){
        const std::vector<uint32_t> tarv=to_normalized_unicode(tar);
        const std::vector<uint32_t> ansv=to_unicode(ans);
        if(!same(tarv, ansv)){
            const std::string tarns=to_string(tarv);
            test_result res;
            res.success = false;
            size_t idx = 0;
            append_str(res.message.buffer, idx, "utf8 hiragana-katakana normalizer test was failed.\n input: ");
            append_str(res.message.buffer, idx, tar);
            append_str(res.message.buffer, idx, ", expected: ");
            append_str(res.message.buffer, idx, ans);
            append_str(res.message.buffer, idx, ", output: ");
            append_str(res.message.buffer, idx, tarns);
            return res;
        }
    }
    return {true,{}};
}

constexpr test_result res = test();
static_assert(res.success, res.message);

} // namespace 

