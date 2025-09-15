#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <manager/users/user_manager.hpp>

namespace USER_MANAGER {
using namespace std;
using isit=istreambuf_iterator<char>;

string UserManager::hash_password_legacy_sha256(const string& password)const {
  EVP_MD_CTX* context = EVP_MD_CTX_new();
  if (context == nullptr) return "";
  if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) {
    EVP_MD_CTX_free(context);
    return "";
  }
  if (EVP_DigestUpdate(context, password.c_str(), password.length()) != 1) {
    EVP_MD_CTX_free(context);
    return "";
  }
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int lengthOfHash = 0;
  if (EVP_DigestFinal_ex(context, hash, &lengthOfHash) != 1) {
    EVP_MD_CTX_free(context);
    return "";
  }
  EVP_MD_CTX_free(context);
  return bytes_to_hex(hash, lengthOfHash);
}

vector<unsigned char> UserManager::hex_to_bytes(const string& hex)const {
  vector<unsigned char> bytes;
  if (hex.size() % 2 != 0) return bytes;
  bytes.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    unsigned int byte;
    stringstream ss;
    ss << std::hex << hex.substr(i, 2);
    ss >> byte;
    bytes.push_back(static_cast<unsigned char>(byte));
  }
  return bytes;
}

string UserManager::generate_salt_hex(size_t num_bytes)const {
  vector<unsigned char> salt(num_bytes);
  if (RAND_bytes(salt.data(), static_cast<int>(num_bytes)) != 1)
    // fallback: pseudo-random (should rarely happen)
    for (size_t i = 0; i < num_bytes; ++i)
      salt[i] = static_cast<unsigned char>(rand() % 256);
  return bytes_to_hex(salt.data(), salt.size());
}

string UserManager::hash_password_pbkdf2_sha256(const string& password, const string& salt_hex, int iterations)const {
  vector<unsigned char> salt = hex_to_bytes(salt_hex);
  if (salt.empty() || iterations <= 0) return "";
  const int keylen = 32; // 256-bit
  const int plen=password.size();
  const int slen=salt.size();
  unsigned char out[keylen];
  if (PKCS5_PBKDF2_HMAC(password.c_str(), plen, salt.data(), slen, iterations, EVP_sha256(), keylen, out) != 1)
    return "";
  return bytes_to_hex(out, keylen);
}

string UserManager::bytes_to_hex(const unsigned char* data, size_t len)const {
  stringstream ss;
  ss << std::hex << std::setfill('0');
  for(size_t i=0;i<len;++i)
    ss << std::setw(2) << static_cast<int>(data[i]);
  return ss.str();
}

string UserManager::get_current_timestamp()const{
  auto now = chrono::system_clock::now();
  auto time_t = chrono::system_clock::to_time_t(now);
  stringstream ss;
  ss << put_time(localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

} // namespace USER_MANAGER