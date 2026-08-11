#include <manager/users/manager.hpp>

#include <chrono>
#include <iomanip>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace USER_MANAGER {

std::vector<unsigned char> UserManager::hex_to_bytes(
    const std::string& hex) const {
  if (hex.empty() || hex.size() % 2 != 0)
    return {};
  std::vector<unsigned char> bytes;
  bytes.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    const auto value = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    const int high = value(hex[i]);
    const int low = value(hex[i + 1]);
    if (high < 0 || low < 0)
      return {};
    bytes.push_back(static_cast<unsigned char>((high << 4) | low));
  }
  return bytes;
}

std::string UserManager::generate_salt_hex(std::size_t num_bytes) const {
  std::vector<unsigned char> salt(num_bytes);
  if (num_bytes == 0 ||
      RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1)
    return {};
  return bytes_to_hex(salt.data(), salt.size());
}

std::string UserManager::hash_password_pbkdf2_sha256(
    const std::string& password,
    const std::string& salt_hex,
    int iterations) const {
  const std::vector<unsigned char> salt = hex_to_bytes(salt_hex);
  if (salt.empty() || iterations <= 0)
    return {};

  std::array<unsigned char, 32> output{};
  if (PKCS5_PBKDF2_HMAC(
          password.data(),
          static_cast<int>(password.size()),
          salt.data(),
          static_cast<int>(salt.size()),
          iterations,
          EVP_sha256(),
          static_cast<int>(output.size()),
          output.data()) != 1)
    return {};
  return bytes_to_hex(output.data(), output.size());
}

std::string UserManager::bytes_to_hex(
    const unsigned char* data,
    std::size_t len) const {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0');
  for (std::size_t i = 0; i < len; ++i)
    stream << std::setw(2) << static_cast<unsigned int>(data[i]);
  return stream.str();
}

bool UserManager::verify_password(
    const User& user,
    const std::string& password) const {
  if (user.password_salt.empty() || user.password_iter <= 0 ||
      user.password_hash.empty())
    return false;
  const std::string actual =
      hash_password_pbkdf2_sha256(password, user.password_salt, user.password_iter);
  if (actual.size() != user.password_hash.size())
    return false;
  return CRYPTO_memcmp(
             actual.data(), user.password_hash.data(), actual.size()) == 0;
}

std::string UserManager::get_current_timestamp() const {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  std::tm local{};
  localtime_r(&time, &local);
  std::ostringstream stream;
  stream << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
  return stream.str();
}

} // namespace USER_MANAGER
