#pragma once

#include <string>
#include <vector>
#include <algorithm>
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

  Params()=default;
  template<class STR1,class STR2,class STR3,class VEC,class STR4,class STR5,class STR6, class VEC2>
  Params(int sstm, int sp, STR1&& cert, STR2&& key, STR3&& jwt, bool isdev, VEC&& origins, STR4&& methods, STR5&& headers, STR6&& viewer, VEC2&& viewer_pub_list)
  :SESSION_TIMEOUT_MINUTES(sstm), SERVER_PORT(sp),
   SSL_CERT_PATH(cert), SSL_KEY_PATH(key), JWT_SECRET_KEY(jwt),
   IS_DEVELOPMENT(isdev),
   ALLOWED_ORIGINS(origins), ALLOWED_METHODS(methods),
   ALLOWED_HEADERS(headers), VIEWER_DIR(viewer),
   VIEWER_PUB_LIST(viewer_pub_list){}
  Params(const Params&)=default;
  Params(Params&&)=default;
  Params& operator=(const Params&)=default;
  Params& operator=(Params&&)=default;
};
// グローバル設定パラメータ
inline Params params;

// 設定ファイル(param.json)からパラメータを読み込む関数
bool load_params(const std::string& filepath = "config/param.json");

// 指定されたoriginが許可されているかチェック
inline bool is_origin_allowed(const std::string& origin) {
  return std::ranges::find(params.ALLOWED_ORIGINS, origin) != params.ALLOWED_ORIGINS.end();
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