#include "manager/auth/jwt.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace JWT {
using namespace std;
using namespace std::chrono;

JWTPayload::JWTPayload(const string& username) {
  auto now = system_clock::now();
  auto now_seconds = duration_cast<seconds>(now.time_since_epoch()).count();
  
  iss = "home-server";
  sub = username;
  aud = "home-server-users";
  iat = now_seconds;
  exp = now_seconds + (CONFIG::params.SESSION_TIMEOUT_MINUTES * 60);
  
  // JWT ID生成（ランダム）
  random_device rd;
  mt19937_64 gen(rd());
  uniform_int_distribution<uint64_t> dis;
  uint64_t random_value = dis(gen);
  
  stringstream ss;
  ss << hex << setw(16) << setfill('0') << random_value;
  jti = ss.str();
}

string base64_encode(const string& input) {
  BIO* bio = BIO_new(BIO_s_mem());
  BIO* b64 = BIO_new(BIO_f_base64());
  bio = BIO_push(b64, bio);
  
  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
  BIO_write(bio, input.c_str(), input.length());
  BIO_flush(bio);
  
  BUF_MEM* bufferPtr;
  BIO_get_mem_ptr(bio, &bufferPtr);
  
  string result(bufferPtr->data, bufferPtr->length);
  
  BIO_free_all(bio);
  return result;
}

string base64_decode(const string& input) {
  BIO* bio = BIO_new_mem_buf(input.c_str(), input.length());
  BIO* b64 = BIO_new(BIO_f_base64());
  bio = BIO_push(b64, bio);
  
  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
  
  char buffer[1024];
  int length = BIO_read(bio, buffer, sizeof(buffer));
  
  BIO_free_all(bio);
  
  if (length > 0) {
    return string(buffer, length);
  }
  return "";
}

string base64url_encode(const string& input) {
  string encoded = base64_encode(input);
  // + を - に、/ を _ に、= を削除
  replace(encoded.begin(), encoded.end(), '+', '-');
  replace(encoded.begin(), encoded.end(), '/', '_');
  encoded.erase(remove(encoded.begin(), encoded.end(), '='), encoded.end());
  return encoded;
}

string base64url_decode(const string& input) {
  string decoded = input;
  // - を + に、_ を / に変換
  replace(decoded.begin(), decoded.end(), '-', '+');
  replace(decoded.begin(), decoded.end(), '_', '/');
  
  // パディングを追加
  int padding = 4 - (decoded.length() % 4);
  if (padding != 4) {
    decoded.append(padding, '=');
  }
  
  return base64_decode(decoded);
}

string json_encode(const JWTPayload& payload) {
  stringstream ss;
  ss << "{"
  << "\"iss\":\"" << payload.iss << "\","
  << "\"sub\":\"" << payload.sub << "\","
  << "\"aud\":\"" << payload.aud << "\","
  << "\"iat\":" << payload.iat << ","
  << "\"exp\":" << payload.exp << ","
  << "\"jti\":\"" << payload.jti << "\""
  << "}";
  return ss.str();
}

string hmac_sha256(const string& key, const string& data) {
  unsigned char hash[32];
  unsigned int hash_len;
  HMAC(EVP_sha256(), key.c_str(), key.length(),
     reinterpret_cast<const unsigned char*>(data.c_str()), data.length(),
     hash, &hash_len);
  return string(reinterpret_cast<char*>(hash), hash_len);
}

string generate_token(const string& username, const string& secret_key) {
  JWTPayload payload(username);
  string header = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
  string payload_str = json_encode(payload);
  
  string header_encoded = base64url_encode(header);
  string payload_encoded = base64url_encode(payload_str);
  
  string data = header_encoded + "." + payload_encoded;
  string signature = hmac_sha256(secret_key, data);
  string signature_encoded = base64url_encode(signature);
  
  return data + "." + signature_encoded;
}

bool verify_token(const string& token, const string& secret_key) {
  size_t first_dot = token.find('.');
  size_t second_dot = token.find('.', first_dot + 1);
  
  if (first_dot == string::npos || second_dot == string::npos) {
    return false;
  }
  
  string header_encoded = token.substr(0, first_dot);
  string payload_encoded = token.substr(first_dot + 1, second_dot - first_dot - 1);
  string signature_encoded = token.substr(second_dot + 1);
  
  string data = header_encoded + "." + payload_encoded;
  string expected_signature = hmac_sha256(secret_key, data);
  string expected_signature_encoded = base64url_encode(expected_signature);
  
  return signature_encoded == expected_signature_encoded;
}

JWTPayload decode_payload(const string& token) {
  size_t first_dot = token.find('.');
  size_t second_dot = token.find('.', first_dot + 1);
  
  if (first_dot == string::npos || second_dot == string::npos) {
    return JWTPayload();
  }
  
  string payload_encoded = token.substr(first_dot + 1, second_dot - first_dot - 1);
  string payload_str = base64url_decode(payload_encoded);
  
  // 簡易的なJSONパース（実際の実装ではより堅牢なパーサーを使用）
  JWTPayload payload;
  
  // "sub":"username" の形式からユーザー名を抽出
  size_t sub_pos = payload_str.find("\"sub\":\"");
  if (sub_pos != string::npos) {
    sub_pos += 7; // "sub":" の長さ
    size_t end_pos = payload_str.find("\"", sub_pos);
    if (end_pos != string::npos) {
      payload.sub = payload_str.substr(sub_pos, end_pos - sub_pos);
    }
  }
  
  // "exp":1234567890 の形式から有効期限を抽出
  size_t exp_pos = payload_str.find("\"exp\":");
  if (exp_pos != string::npos) {
    exp_pos += 6; // "exp": の長さ
    size_t end_pos = payload_str.find_first_of(",}", exp_pos);
    if (end_pos != string::npos) {
      string exp_str = payload_str.substr(exp_pos, end_pos - exp_pos);
      try {
        payload.exp = stoll(exp_str);
      } catch (...) {
        payload.exp = 0;
      }
    }
  }
  
  return payload;
}

bool is_token_expired(const string& token) {
  JWTPayload payload = decode_payload(token);
  auto now = system_clock::now();
  auto now_seconds = duration_cast<seconds>(now.time_since_epoch()).count();
  
  return now_seconds > payload.exp;
}

string get_username_from_token(const string& token) {
  JWTPayload payload = decode_payload(token);
  return payload.sub;
}
} // namespace JWT