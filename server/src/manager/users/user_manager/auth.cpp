#include <manager/users/manager.hpp>

#include <ranges>

namespace USER_MANAGER {

bool UserManager::authenticate_user(
    const std::string& username,
    const std::string& password) {
  User snapshot;
  {
    std::lock_guard lock(users_mutex);
    if (!initialized)
      return false;
    const User* user = find_user_locked(username);
    if (!user)
      return false;
    snapshot = *user;
  }

  if (!verify_password(snapshot, password))
    return false;

  std::lock_guard lock(users_mutex);
  User* user = find_user_locked(username);
  if (!user ||
      user->password_hash != snapshot.password_hash ||
      user->password_salt != snapshot.password_salt ||
      user->password_iter != snapshot.password_iter)
    return false;

  const auto previous = users;
  user->last_login = get_current_timestamp();
  if (!save_users_locked())
    users = previous;
  return true;
}

bool UserManager::is_admin(const std::string& username) {
  std::lock_guard lock(users_mutex);
  const User* user = find_user_locked(username);
  return user && user->role == "admin";
}

bool UserManager::can_register_admin(const std::string& username) {
  return is_admin(username);
}

bool UserManager::can_register_user(const std::string& username) {
  return is_admin(username);
}

bool UserManager::can_manage_users(const std::string& username) {
  return is_admin(username);
}

bool UserManager::is_first_user() {
  std::lock_guard lock(users_mutex);
  return initialized && users.empty();
}

bool UserManager::user_exists(const std::string& username) {
  std::lock_guard lock(users_mutex);
  return find_user_locked(username) != nullptr;
}

bool UserManager::session_matches(
    const std::string& username,
    std::uint64_t generation) {
  std::lock_guard lock(users_mutex);
  const User* user = find_user_locked(username);
  return user && user->session_generation == generation;
}

std::optional<std::uint64_t> UserManager::get_session_generation(
    const std::string& username) {
  std::lock_guard lock(users_mutex);
  const User* user = find_user_locked(username);
  if (!user)
    return std::nullopt;
  return user->session_generation;
}

} // namespace USER_MANAGER
