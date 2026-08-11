#include <manager/users/manager.hpp>

#include <algorithm>
#include <limits>
#include <ranges>

#include <manager/auth/login_guard.hpp>

namespace USER_MANAGER {

User* UserManager::find_user_locked(const std::string& username) {
  const auto it = std::ranges::find_if(
      users, [&username](const User& user) { return user.username == username; });
  return it == users.end() ? nullptr : &*it;
}

const User* UserManager::find_user_locked(const std::string& username) const {
  const auto it = std::ranges::find_if(
      users, [&username](const User& user) { return user.username == username; });
  return it == users.end() ? nullptr : &*it;
}

std::size_t UserManager::admin_count_locked() const {
  return static_cast<std::size_t>(std::ranges::count_if(
      users, [](const User& user) { return user.role == "admin"; }));
}

bool UserManager::bump_session_generation(User& user) const {
  if (user.session_generation == std::numeric_limits<std::uint64_t>::max())
    return false;
  ++user.session_generation;
  return true;
}

MutationResult UserManager::add_user(
    const std::string& username,
    const std::string& password,
    const std::string& role,
    const std::string& created_by) {
  if (username.empty() || password.empty())
    return MutationResult::invalid_input;
  if (role != "admin" && role != "user")
    return MutationResult::invalid_role;

  const std::string salt = generate_salt_hex();
  const std::string hash =
      hash_password_pbkdf2_sha256(password, salt, DEFAULT_PBKDF2_ITERATIONS);
  if (salt.empty() || hash.empty())
    return MutationResult::crypto_failed;

  std::lock_guard lock(users_mutex);
  if (!initialized)
    return MutationResult::not_initialized;
  if (find_user_locked(username))
    return MutationResult::already_exists;

  const bool first_user = users.empty();
  if (first_user) {
    if (role != "admin" ||
        (!created_by.empty() && created_by != "system"))
      return MutationResult::forbidden;
  } else {
    const User* actor = find_user_locked(created_by);
    if (!actor)
      return MutationResult::invalid_actor;
    if (actor->role != "admin")
      return MutationResult::forbidden;
  }

  User new_user;
  new_user.username = username;
  new_user.password_hash = hash;
  new_user.password_salt = salt;
  new_user.password_iter = DEFAULT_PBKDF2_ITERATIONS;
  new_user.role = role;
  new_user.created_by = first_user ? "system" : created_by;
  new_user.created_at = get_current_timestamp();

  const auto previous = users;
  users.push_back(std::move(new_user));
  if (!save_users_locked()) {
    users = previous;
    return MutationResult::save_failed;
  }
  return MutationResult::success;
}

MutationResult UserManager::delete_user(
    const std::string& username,
    const std::string& deleted_by) {
  {
    std::lock_guard lock(users_mutex);
    if (!initialized)
      return MutationResult::not_initialized;
    const User* actor = find_user_locked(deleted_by);
    if (!actor)
      return MutationResult::invalid_actor;
    if (actor->role != "admin")
      return MutationResult::forbidden;

    const auto it = std::ranges::find_if(
        users, [&username](const User& user) { return user.username == username; });
    if (it == users.end())
      return MutationResult::not_found;
    if (it->role == "admin" && admin_count_locked() <= 1)
      return MutationResult::last_admin;

    const auto previous = users;
    users.erase(it);
    if (!save_users_locked()) {
      users = previous;
      return MutationResult::save_failed;
    }
    AUTH::login_attempt_guard().reset_user(username);
  }
  return MutationResult::success;
}

MutationResult UserManager::promote_user(
    const std::string& username,
    const std::string& promoted_by) {
  return change_user_role(username, "admin", promoted_by);
}

MutationResult UserManager::demote_user(
    const std::string& username,
    const std::string& demoted_by) {
  return change_user_role(username, "user", demoted_by);
}

MutationResult UserManager::change_user_role(
    const std::string& username,
    const char* role,
    const std::string& changed_by) {
  std::lock_guard lock(users_mutex);
  if (!initialized)
    return MutationResult::not_initialized;
  const User* actor = find_user_locked(changed_by);
  if (!actor)
    return MutationResult::invalid_actor;
  if (actor->role != "admin")
    return MutationResult::forbidden;
  User* target = find_user_locked(username);
  if (!target)
    return MutationResult::not_found;
  if (target->role == role)
    return MutationResult::success;
  if (target->role == "admin" && std::string_view(role) == "user" &&
      admin_count_locked() <= 1)
    return MutationResult::last_admin;

  const auto previous = users;
  if (!bump_session_generation(*target)) {
    users = previous;
    return MutationResult::save_failed;
  }
  target = find_user_locked(username);
  target->role = role;
  if (!save_users_locked()) {
    users = previous;
    return MutationResult::save_failed;
  }
  return MutationResult::success;
}

MutationResult UserManager::change_password(
    const std::string& username,
    const std::string& current_password,
    const std::string& new_password) {
  User snapshot;
  {
    std::lock_guard lock(users_mutex);
    if (!initialized)
      return MutationResult::not_initialized;
    const User* user = find_user_locked(username);
    if (!user)
      return MutationResult::not_found;
    snapshot = *user;
  }
  if (!verify_password(snapshot, current_password))
    return MutationResult::invalid_credentials;

  const std::string salt = generate_salt_hex();
  const std::string hash =
      hash_password_pbkdf2_sha256(new_password, salt, DEFAULT_PBKDF2_ITERATIONS);
  if (salt.empty() || hash.empty())
    return MutationResult::crypto_failed;

  std::lock_guard lock(users_mutex);
  User* user = find_user_locked(username);
  if (!user)
    return MutationResult::not_found;
  if (user->password_hash != snapshot.password_hash ||
      user->password_salt != snapshot.password_salt ||
      user->password_iter != snapshot.password_iter)
    return MutationResult::invalid_credentials;

  const auto previous = users;
  user->password_hash = hash;
  user->password_salt = salt;
  user->password_iter = DEFAULT_PBKDF2_ITERATIONS;
  if (!bump_session_generation(*user) || !save_users_locked()) {
    users = previous;
    return MutationResult::save_failed;
  }
  return MutationResult::success;
}

MutationResult UserManager::invalidate_all_sessions(const std::string& username) {
  std::lock_guard lock(users_mutex);
  if (!initialized)
    return MutationResult::not_initialized;
  User* user = find_user_locked(username);
  if (!user)
    return MutationResult::not_found;
  const auto previous = users;
  if (!bump_session_generation(*user) || !save_users_locked()) {
    users = previous;
    return MutationResult::save_failed;
  }
  return MutationResult::success;
}

std::vector<User> UserManager::get_all_users() {
  std::lock_guard lock(users_mutex);
  return users;
}

std::string UserManager::get_user_role(const std::string& username) {
  std::lock_guard lock(users_mutex);
  const User* user = find_user_locked(username);
  return user ? user->role : std::string();
}

bool UserManager::is_initialized() {
  std::lock_guard lock(users_mutex);
  return initialized;
}

std::string UserManager::users_file_path() {
  std::lock_guard lock(users_mutex);
  return users_file.string();
}

} // namespace USER_MANAGER
