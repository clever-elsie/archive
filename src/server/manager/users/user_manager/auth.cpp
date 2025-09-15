#include <ranges>

#include <manager/users/user_manager.hpp>

namespace USER_MANAGER {
using namespace std;

bool UserManager::authenticate_user(const string& username, const string& password) {
  lock_guard<mutex> lock(users_mutex);
  auto it = ranges::find_if(users, [&username](const User& user) { return user.username == username; });
  if (it == users.end()) return false;
  
  bool authenticated = false;
  if (!it->password_salt.empty() && it->password_iter > 0) {
    // PBKDF2 path
    string input_hash = hash_password_pbkdf2_sha256(password, it->password_salt, it->password_iter);
    if (!input_hash.empty() && it->password_hash == input_hash) {
      authenticated = true;
    }
  } else {
    // Legacy SHA-256 path
    string input_hash = hash_password_legacy_sha256(password);
    if (it->password_hash == input_hash) {
      authenticated = true;
      // migrate to PBKDF2
      it->password_salt = generate_salt_hex();
      it->password_iter = DEFAULT_PBKDF2_ITERATIONS;
      it->password_hash = hash_password_pbkdf2_sha256(password, it->password_salt, it->password_iter);
      save_users();
    }
  }
  if (authenticated) {
    update_last_login(username);
    return true;
  }
  return false;
}

bool UserManager::is_admin(const string& username){
  lock_guard<mutex> lock(users_mutex);
  auto it = ranges::find_if(users, [&username](const User& user) { return user.username == username; });
  return (it != users.end() && it->role == "admin");
}

bool UserManager::can_register_user(const string& username){
  return is_admin(username);
}

bool UserManager::can_manage_users(const string& username){
  return is_admin(username);
}

bool UserManager::can_register_admin(const string& username){
  return is_first_user();
}

bool UserManager::is_first_user(){
  lock_guard<mutex> lock(users_mutex);
  return users.empty();
}

bool UserManager::user_exists(const string& username){
  lock_guard<mutex> lock(users_mutex);
  auto it = ranges::find_if(users, [&username](const User& user) { return user.username == username; });
  return it != users.end();
}

} // namespace USER_MANAGER