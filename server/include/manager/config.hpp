#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <ranges>
#include <utility>

namespace CONFIG {
struct Params {
  int SESSION_TIMEOUT_MINUTES;
  int SERVER_PORT;
  std::string SSL_CERT_PATH;
  std::string SSL_KEY_PATH;
  std::string JWT_SECRET_KEY;
  bool IS_DEVELOPMENT;
  std::vector<std::string> ALLOWED_ORIGINS;
  std::string ALLOWED_METHODS;
  std::string ALLOWED_HEADERS;
  std::string VIEWER_DIR;
  std::vector<std::string> VIEWER_PUB_LIST;
  std::string USER_STORE_PATH = "users.json";
  int VIEWER_SCAN_INTERVAL_SECONDS = 3 * 60 * 60;

  Params()=default;
  template<class STR1,class STR2,class STR3,class VEC,class STR4,class STR5,class STR6, class VEC2, class STR7>
  Params(int sstm, int sp, STR1&& cert, STR2&& key, STR3&& jwt, bool isdev, VEC&& origins, STR4&& methods, STR5&& headers, STR6&& viewer, VEC2&& viewer_pub_list, STR7&& user_store)
  :SESSION_TIMEOUT_MINUTES(sstm), SERVER_PORT(sp),
   SSL_CERT_PATH(cert), SSL_KEY_PATH(key), JWT_SECRET_KEY(jwt),
   IS_DEVELOPMENT(isdev),
   ALLOWED_ORIGINS(origins), ALLOWED_METHODS(methods),
   ALLOWED_HEADERS(headers), VIEWER_DIR(viewer),
   VIEWER_PUB_LIST(viewer_pub_list), USER_STORE_PATH(user_store){}
  Params(const Params&)=default;
  Params(Params&&)=default;
  Params& operator=(const Params&)=default;
  Params& operator=(Params&&)=default;
};
// グローバル設定パラメータ
inline Params params;

// 設定ファイル(param.json)からパラメータを読み込む関数
bool load_params(const std::string& filepath = "config/param.json");
std::string config_path_from_args(int argc, char* argv[]);

inline std::string normalize_origin(std::string origin) {
  while (!origin.empty() && origin.back() == '/')
    origin.pop_back();

  const auto scheme_end = origin.find("://");
  if (scheme_end == std::string::npos)
    return origin;

  std::string scheme = origin.substr(0, scheme_end);
  std::ranges::transform(scheme, scheme.begin(), [](unsigned char value) {
    return static_cast<char>(std::tolower(value));
  });
  const auto authority_end = origin.find_first_of("/?#", scheme_end + 3);
  std::string authority = origin.substr(
      scheme_end + 3,
      authority_end == std::string::npos
          ? std::string::npos
          : authority_end - scheme_end - 3);
  std::ranges::transform(authority, authority.begin(), [](unsigned char value) {
    return static_cast<char>(std::tolower(value));
  });

  if ((scheme == "https" && authority.ends_with(":443")) ||
      (scheme == "http" && authority.ends_with(":80")))
    authority.erase(authority.size() - 4);

  // Originにはpath/query/fragmentを含めない。ここでは比較値を一意化し、
  // そのような設定値は従来どおり一致しないままにする。
  const std::string suffix =
      authority_end == std::string::npos ? std::string()
                                         : origin.substr(authority_end);
  return scheme + "://" + authority + suffix;
}

// 指定されたoriginが許可されているかチェック
inline bool is_origin_allowed(const std::string& origin) {
  const auto normalized = normalize_origin(origin);
  return std::ranges::any_of(
      params.ALLOWED_ORIGINS,
      [&normalized](const std::string& allowed) {
        return normalize_origin(allowed) == normalized;
      });
}

// 許可されたoriginのリストを取得
inline const std::vector<std::string>& get_allowed_origins() {
  return params.ALLOWED_ORIGINS;
}

// 許可されたoriginをカンマ区切りの文字列として取得（CORSヘッダー用）
inline std::string get_origins_header() {
  std::string result;
  for (size_t i = 0; i < params.ALLOWED_ORIGINS.size(); ++i) {
    if (i > 0) result += ", ";
    result += params.ALLOWED_ORIGINS[i];
  }
  return result;
}

}
