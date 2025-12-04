#include <ranges>

#include <manager/users/manager.hpp>

namespace USER_MANAGER {
using namespace std;

bool UserManager::add_user(const string& username, const string& password, const string& role, const string& created_by) {
  if (user_exists(username)) return false; // ユーザーが既に存在
  
  User new_user;
  new_user.username = username;
  new_user.password_salt = generate_salt_hex();
  new_user.password_iter = DEFAULT_PBKDF2_ITERATIONS;
  new_user.password_hash = hash_password_pbkdf2_sha256(password, new_user.password_salt, new_user.password_iter);
  new_user.role = role;
  new_user.created_by = created_by;
  new_user.created_at = get_current_timestamp();
  new_user.last_login = "";
  
  lock_guard<mutex> lock(users_mutex);
  users.push_back(new_user);
  cout << "UserManager: Added user '" << username << "' with role '" << role << "'" << endl;
  return save_users(); // 即座に保存
}

bool UserManager::delete_user(const string& username, const string& deleted_by) {
  if (!can_manage_users(deleted_by)) return false; // 権限なし
  
  lock_guard<mutex> lock(users_mutex);
  auto it = ranges::find_if(users, [&username](const User& user) { return user.username == username; });
  
  if (it == users.end()) return false; // ユーザーが存在しない
  
  if (it->role == "admin" && !is_admin(deleted_by))
    return false; // 一般ユーザーは管理者を削除できない
  
  users.erase(it);
  cout << "UserManager: Deleted user '" << username << "' by '" << deleted_by << "'" << endl;
  return save_users(); // 即座に保存
}

bool UserManager::change_user_role(const string&username, const char*const role, const string& changed_by){
  if(!is_admin(changed_by)) return false; // 管理者のみ変更可能
  lock_guard<mutex> lock(users_mutex);
  auto it=ranges::find_if(users, [&username](const User& user) { return user.username == username; });
  if(it==users.end()) return false; // ユーザーが存在しない
  it->role=role;
  cout << "UserManager: "<<(role=="admin"?"Promoted":"Demoted")<<" user '" << username << "' role to '" << role << "' by '" << changed_by << "'" << endl;
  return save_users(); // 即座に保存
}

bool UserManager::promote_user(const std::string& username, const std::string& promoted_by) {
  return change_user_role(username, "admin", promoted_by);
}

bool UserManager::demote_user(const std::string& username, const std::string& demoted_by) {
  return change_user_role(username, "user", demoted_by);
}

std::vector<User> UserManager::get_all_users() {
  lock_guard<mutex> lock(users_mutex);
  return users;
}

string UserManager::get_user_role(const string& username) {
  lock_guard<mutex> lock(users_mutex);
  auto it = ranges::find_if(users, [&username](const User& user) { return user.username == username; });
  if (it != users.end()) return it->role;
  return "";
}

void UserManager::update_last_login(const string& username) {
  auto it = ranges::find_if(users, [&username](const User& user) { return user.username == username; });
  
  if (it != users.end()) {
    it->last_login = get_current_timestamp();
    save_users(); // 即座に保存
  }
}

void UserManager::update_last_login_locked(const string& username) {
  lock_guard<mutex> lock(users_mutex);
  update_last_login(username);
}
} // namespace USER_MANAGER