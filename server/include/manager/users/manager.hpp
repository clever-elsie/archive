#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <crow/json.h>

namespace USER_MANAGER {

enum class MutationResult {
  success,
  not_initialized,
  invalid_input,
  invalid_role,
  invalid_actor,
  invalid_credentials,
  forbidden,
  not_found,
  already_exists,
  last_admin,
  save_failed,
  crypto_failed
};

struct User {
  std::string username;
  std::string password_hash;
  std::string password_salt;
  int password_iter = 0;
  std::string role;
  std::string created_by;
  std::string created_at;
  std::string last_login;
  std::uint64_t session_generation = 0;

  User() = default;
  explicit User(const crow::json::rvalue& json);
  crow::json::wvalue to_json() const;
};

class UserManager {
  static constexpr int DEFAULT_PBKDF2_ITERATIONS = 200000;

  std::vector<User> users;
  std::filesystem::path users_file;
  bool initialized = false;
  mutable std::mutex users_mutex;

  std::vector<unsigned char> hex_to_bytes(const std::string& hex) const;
  std::string generate_salt_hex(std::size_t num_bytes = 16) const;
  std::string hash_password_pbkdf2_sha256(
      const std::string& password,
      const std::string& salt_hex,
      int iterations) const;
  std::string bytes_to_hex(const unsigned char* data, std::size_t len) const;
  std::string get_current_timestamp() const;
  bool verify_password(const User& user, const std::string& password) const;
  bool load_users_locked();
  bool save_users_locked();
  std::size_t admin_count_locked() const;
  User* find_user_locked(const std::string& username);
  const User* find_user_locked(const std::string& username) const;
  bool bump_session_generation(User& user) const;
  MutationResult change_user_role(
      const std::string& username,
      const char* role,
      const std::string& changed_by);

public:
  UserManager() = default;
  UserManager(const UserManager&) = delete;
  UserManager& operator=(const UserManager&) = delete;

  bool initialize(const std::string& path);
  bool load_users();
  bool save_users();

  MutationResult add_user(
      const std::string& username,
      const std::string& password,
      const std::string& role,
      const std::string& created_by);
  MutationResult delete_user(
      const std::string& username,
      const std::string& deleted_by);
  MutationResult promote_user(
      const std::string& username,
      const std::string& promoted_by);
  MutationResult demote_user(
      const std::string& username,
      const std::string& demoted_by);
  MutationResult change_password(
      const std::string& username,
      const std::string& current_password,
      const std::string& new_password);
  MutationResult invalidate_all_sessions(const std::string& username);

  bool authenticate_user(const std::string& username, const std::string& password);
  bool is_admin(const std::string& username);
  bool can_register_admin(const std::string& username);
  bool can_register_user(const std::string& username);
  bool can_manage_users(const std::string& username);
  bool is_first_user();
  bool user_exists(const std::string& username);
  bool session_matches(const std::string& username, std::uint64_t generation);
  std::optional<std::uint64_t> get_session_generation(const std::string& username);

  std::vector<User> get_all_users();
  std::string get_user_role(const std::string& username);
  bool is_initialized();
  std::string users_file_path();
};

inline UserManager& get_user_manager() {
  static UserManager manager;
  return manager;
}

} // namespace USER_MANAGER
