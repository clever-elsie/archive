#pragma once
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

#include <manager/config.hpp>

namespace JWT {
using namespace std;

// JWTペイロード構造体
struct JWTPayload {
  string alg;
  string iss;   // 発行者
  string sub;   // サブジェクト（ユーザー名）
  string aud;   // 対象者
  int64_t iat;  // 発行時刻
  int64_t exp;  // 有効期限
  string jti;   // JWT ID
  uint64_t session_generation = 0;
  bool parsed = false;
  JWTPayload() = default;
  JWTPayload(const string& username, uint64_t generation = 0);
};

string base64_encode(const string& input);
string base64_decode(const string& input);
string base64url_encode(const string& input);
string base64url_decode(const string& input);

string json_encode(const JWTPayload& payload); // JSONエンコード（簡易版）
string hmac_sha256(const string& key, const string& data); // HMAC-SHA256署名生成
string generate_token(const string& username, const string& secret_key, uint64_t generation);

bool verify_token(const string& token, const string& secret_key) ;

JWTPayload decode_payload(const string& token);

bool is_token_expired(const string& token);

// トークンからユーザー名を取得
string get_username_from_token(const string& token);

} // namespace JWT
