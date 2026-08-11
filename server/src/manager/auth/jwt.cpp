#include <manager/auth/jwt.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

namespace JWT {

namespace {

constexpr std::string_view HEADER = R"({"alg":"HS256","typ":"JWT"})";
constexpr std::string_view ISSUER = "home-server";
constexpr std::string_view AUDIENCE = "home-server-users";
constexpr char BASE64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

std::string random_hex(std::size_t bytes) {
  std::vector<unsigned char> data(bytes);
  if (bytes == 0 || RAND_bytes(data.data(), static_cast<int>(bytes)) != 1)
    return {};
  std::string result;
  result.reserve(bytes * 2);
  constexpr char hex[] = "0123456789abcdef";
  for (const unsigned char value : data) {
    result.push_back(hex[value >> 4]);
    result.push_back(hex[value & 0x0f]);
  }
  return result;
}

bool split_token(
    const std::string& token,
    std::string& header,
    std::string& payload,
    std::string& signature) {
  const std::size_t first = token.find('.');
  const std::size_t second =
      first == std::string::npos ? std::string::npos : token.find('.', first + 1);
  if (first == std::string::npos || second == std::string::npos ||
      first == 0 || second == first + 1 || second + 1 >= token.size() ||
      token.find('.', second + 1) != std::string::npos)
    return false;
  header = token.substr(0, first);
  payload = token.substr(first + 1, second - first - 1);
  signature = token.substr(second + 1);
  return true;
}

bool valid_string(const crow::json::rvalue& value) {
  return value.t() == crow::json::type::String;
}

bool valid_number(const crow::json::rvalue& value) {
  return value.t() == crow::json::type::Number;
}

std::optional<std::int64_t> integer_claim(
    const crow::json::rvalue& value) {
  if (!valid_number(value)) return std::nullopt;
  try {
    if (value.nt() == crow::json::num_type::Signed_integer)
      return value.i();
    if (value.nt() == crow::json::num_type::Unsigned_integer) {
      const auto result = value.u();
      if (result > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        return std::nullopt;
      return static_cast<std::int64_t>(result);
    }
  } catch (...) {
  }
  return std::nullopt;
}

std::optional<std::uint64_t> unsigned_integer_claim(
    const crow::json::rvalue& value) {
  if (!valid_number(value)) return std::nullopt;
  try {
    if (value.nt() == crow::json::num_type::Unsigned_integer)
      return value.u();
    if (value.nt() == crow::json::num_type::Signed_integer) {
      const auto result = value.i();
      if (result < 0) return std::nullopt;
      return static_cast<std::uint64_t>(result);
    }
  } catch (...) {
  }
  return std::nullopt;
}

bool valid_jti(std::string_view value) {
  if (value.size() != 32)
    return false;
  return std::ranges::all_of(value, [](const char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
  });
}

} // namespace

JWTPayload::JWTPayload(const std::string& username, std::uint64_t generation) {
  const auto now = std::chrono::system_clock::now();
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
      now.time_since_epoch()).count();
  alg = "HS256";
  iss = std::string(ISSUER);
  sub = username;
  aud = std::string(AUDIENCE);
  iat = seconds;
  exp = seconds +
        static_cast<std::int64_t>(CONFIG::params.SESSION_TIMEOUT_MINUTES) * 60;
  jti = random_hex(16);
  session_generation = generation;
  parsed = true;
}

std::string base64_encode(const std::string& input) {
  std::string output;
  output.reserve((input.size() + 2) / 3 * 4);
  for (std::size_t i = 0; i < input.size(); i += 3) {
    const std::uint32_t value =
        static_cast<std::uint32_t>(static_cast<unsigned char>(input[i])) << 16 |
        (i + 1 < input.size()
             ? static_cast<std::uint32_t>(
                   static_cast<unsigned char>(input[i + 1])) << 8
             : 0) |
        (i + 2 < input.size()
             ? static_cast<std::uint32_t>(
                   static_cast<unsigned char>(input[i + 2]))
             : 0);
    output.push_back(BASE64_TABLE[(value >> 18) & 0x3f]);
    output.push_back(BASE64_TABLE[(value >> 12) & 0x3f]);
    output.push_back(i + 1 < input.size() ? BASE64_TABLE[(value >> 6) & 0x3f]
                                          : '=');
    output.push_back(i + 2 < input.size() ? BASE64_TABLE[value & 0x3f] : '=');
  }
  return output;
}

std::string base64_decode(const std::string& input) {
  if (input.empty() || input.size() % 4 != 0)
    return {};
  std::string output;
  output.reserve(input.size() / 4 * 3);
  for (std::size_t i = 0; i < input.size(); i += 4) {
    const int a = base64_value(input[i]);
    const int b = base64_value(input[i + 1]);
    const int c = input[i + 2] == '=' ? 0 : base64_value(input[i + 2]);
    const int d = input[i + 3] == '=' ? 0 : base64_value(input[i + 3]);
    if (a < 0 || b < 0 || c < 0 || d < 0 ||
        (input[i + 2] == '=' && input[i + 3] != '=') ||
        (input[i + 2] == '=' && i + 4 != input.size()) ||
        (input[i + 3] == '=' && i + 4 != input.size()))
      return {};
    const std::uint32_t value =
        static_cast<std::uint32_t>(a) << 18 |
        static_cast<std::uint32_t>(b) << 12 |
        static_cast<std::uint32_t>(c) << 6 |
        static_cast<std::uint32_t>(d);
    output.push_back(static_cast<char>((value >> 16) & 0xff));
    if (input[i + 2] != '=')
      output.push_back(static_cast<char>((value >> 8) & 0xff));
    if (input[i + 3] != '=')
      output.push_back(static_cast<char>(value & 0xff));
  }
  return output;
}

std::string base64url_encode(const std::string& input) {
  std::string output = base64_encode(input);
  std::replace(output.begin(), output.end(), '+', '-');
  std::replace(output.begin(), output.end(), '/', '_');
  while (!output.empty() && output.back() == '=')
    output.pop_back();
  return output;
}

std::string base64url_decode(const std::string& input) {
  if (input.empty() || input.find_first_not_of(
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_")
                           != std::string::npos ||
      input.size() % 4 == 1)
    return {};
  std::string padded = input;
  std::replace(padded.begin(), padded.end(), '-', '+');
  std::replace(padded.begin(), padded.end(), '_', '/');
  while (padded.size() % 4 != 0)
    padded.push_back('=');
  return base64_decode(padded);
}

std::string json_encode(const JWTPayload& payload) {
  crow::json::wvalue json;
  json["alg"] = payload.alg;
  json["iss"] = payload.iss;
  json["sub"] = payload.sub;
  json["aud"] = payload.aud;
  json["iat"] = payload.iat;
  json["exp"] = payload.exp;
  json["jti"] = payload.jti;
  json["session_generation"] = payload.session_generation;
  return json.dump();
}

std::string hmac_sha256(const std::string& key, const std::string& data) {
  std::array<unsigned char, EVP_MAX_MD_SIZE> hash{};
  unsigned int hash_length = 0;
  if (!HMAC(
          EVP_sha256(),
          key.data(),
          static_cast<int>(key.size()),
          reinterpret_cast<const unsigned char*>(data.data()),
          data.size(),
          hash.data(),
          &hash_length))
    return {};
  return std::string(reinterpret_cast<const char*>(hash.data()), hash_length);
}

std::string generate_token(
    const std::string& username,
    const std::string& secret_key,
    std::uint64_t generation) {
  const JWTPayload payload(username, generation);
  if (payload.jti.empty() || payload.exp <= payload.iat || secret_key.empty())
    return {};
  const std::string header_encoded = base64url_encode(std::string(HEADER));
  const std::string payload_encoded = base64url_encode(json_encode(payload));
  const std::string signing_input = header_encoded + "." + payload_encoded;
  const std::string signature = hmac_sha256(secret_key, signing_input);
  if (signature.empty())
    return {};
  return signing_input + "." + base64url_encode(signature);
}

JWTPayload decode_payload(const std::string& token) {
  std::string header_encoded;
  std::string payload_encoded;
  std::string signature_encoded;
  JWTPayload payload;
  if (!split_token(
          token, header_encoded, payload_encoded, signature_encoded))
    return payload;
  const std::string payload_json = base64url_decode(payload_encoded);
  const auto json = crow::json::load(payload_json);
  if (!json || !valid_string(json["iss"]) || !valid_string(json["sub"]) ||
      !valid_string(json["aud"]) || !valid_string(json["jti"]) ||
      !valid_jti(std::string(json["jti"].s())))
    return payload;
  try {
    const auto iat = integer_claim(json["iat"]);
    const auto exp = integer_claim(json["exp"]);
    const auto generation = unsigned_integer_claim(json["session_generation"]);
    if (!iat || !exp || !generation || *iat <= 0 || *exp <= *iat)
      return JWTPayload();
    payload.iss = json["iss"].s();
    payload.sub = json["sub"].s();
    payload.aud = json["aud"].s();
    payload.iat = *iat;
    payload.exp = *exp;
    payload.jti = json["jti"].s();
    payload.session_generation = *generation;
    payload.parsed = true;
  } catch (...) {
    return JWTPayload();
  }
  return payload;
}

bool verify_token(const std::string& token, const std::string& secret_key) {
  std::string header_encoded;
  std::string payload_encoded;
  std::string signature_encoded;
  if (secret_key.empty() ||
      !split_token(token, header_encoded, payload_encoded, signature_encoded))
    return false;

  const std::string header_json = base64url_decode(header_encoded);
  const auto header = crow::json::load(header_json);
  if (!header || !header.has("alg") || !header.has("typ") ||
      !valid_string(header["alg"]) || !valid_string(header["typ"]) ||
      header["alg"].s() != "HS256" || header["typ"].s() != "JWT")
    return false;
  if (!decode_payload(token).parsed)
    return false;

  const std::string actual = base64url_decode(signature_encoded);
  const std::string expected =
      hmac_sha256(secret_key, header_encoded + "." + payload_encoded);
  return actual.size() == expected.size() && !actual.empty() &&
         CRYPTO_memcmp(actual.data(), expected.data(), expected.size()) == 0;
}

bool is_token_expired(const std::string& token) {
  const JWTPayload payload = decode_payload(token);
  if (!payload.parsed)
    return true;
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  return now >= payload.exp;
}

std::string get_username_from_token(const std::string& token) {
  const JWTPayload payload = decode_payload(token);
  return payload.parsed ? payload.sub : std::string();
}

} // namespace JWT
