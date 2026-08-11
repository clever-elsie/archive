#include <manager/users/manager.hpp>

#include <cctype>
#include <chrono>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace USER_MANAGER {

namespace {

bool valid_username(const std::string& username) {
  if (username.empty() || username.size() > 32)
    return false;
  return std::ranges::all_of(username, [](unsigned char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9');
  });
}

bool valid_hex(const std::string& value, std::size_t expected_size) {
  if (value.size() != expected_size)
    return false;
  return std::ranges::all_of(value, [](const char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
  });
}

bool valid_user_record(const User& user, std::string& reason) {
  if (user.username.empty()) {
    reason = "username is empty";
    return false;
  }
  if (user.username.size() > 32) {
    reason = "username exceeds 32 characters";
    return false;
  }
  if (!valid_username(user.username)) {
    reason = "username must contain only ASCII letters and digits";
    return false;
  }
  if (user.password_hash.size() != 64) {
    reason = "password_hash has invalid length (expected 64 hexadecimal characters)";
    return false;
  }
  if (!valid_hex(user.password_hash, 64)) {
    reason = "password_hash contains a non-hexadecimal character";
    return false;
  }
  if (user.password_salt.size() != 32) {
    reason = "password_salt has invalid length (expected 32 hexadecimal characters)";
    return false;
  }
  if (!valid_hex(user.password_salt, 32)) {
    reason = "password_salt contains a non-hexadecimal character";
    return false;
  }
  if (user.password_iter < 100000 || user.password_iter > 2000000) {
    reason = "password_iter is outside the allowed range [100000, 2000000]: " +
             std::to_string(user.password_iter);
    return false;
  }
  if (user.role != "admin" && user.role != "user") {
    reason = "role must be 'admin' or 'user'";
    return false;
  }
  return true;
}

std::optional<std::filesystem::path> resolve_store_target(
    const std::filesystem::path& configured,
    std::string& reason) {
  std::filesystem::path current = configured;
  constexpr std::size_t MAX_SYMLINK_DEPTH = 40;
  for (std::size_t depth = 0; depth < MAX_SYMLINK_DEPTH; ++depth) {
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(current, ec);
    if (ec) {
      reason = "cannot inspect '" + current.string() + "': " + ec.message();
      return std::nullopt;
    }
    if (status.type() != std::filesystem::file_type::symlink)
      return current.lexically_normal();

    const auto link = std::filesystem::read_symlink(current, ec);
    if (ec) {
      reason = "cannot read symbolic link '" + current.string() + "': " +
               ec.message();
      return std::nullopt;
    }
    current = (link.is_absolute() ? link : current.parent_path() / link)
                  .lexically_normal();
  }
  reason = "symbolic link resolution exceeded the maximum depth";
  return std::nullopt;
}

} // namespace

bool UserManager::initialize(const std::string& path) {
  std::lock_guard lock(users_mutex);
  if (path.empty()) {
    std::cerr << "UserManager: failed to initialize user store: path is empty"
              << std::endl;
    return false;
  }
  users_file = std::filesystem::path(path);
  initialized = false;

  std::error_code ec;
  const auto parent = users_file.parent_path();
  if (!parent.empty()) {
    const bool parent_exists = std::filesystem::exists(parent, ec);
    if (ec) {
      std::cerr << "UserManager: failed to initialize user store '"
                << users_file.string() << "': cannot inspect parent directory '"
                << parent.string() << "': " << ec.message() << std::endl;
      return false;
    }
    if (!parent_exists) {
      std::cerr << "UserManager: failed to initialize user store '"
                << users_file.string() << "': parent directory does not exist: "
                << parent.string() << std::endl;
      return false;
    }
    ec.clear();
    if (!std::filesystem::is_directory(parent, ec) || ec) {
      std::cerr << "UserManager: failed to initialize user store '"
                << users_file.string() << "': parent path is not a directory: "
                << parent.string();
      if (ec) std::cerr << ": " << ec.message();
      std::cerr << std::endl;
      return false;
    }
  }
  return load_users_locked();
}

bool UserManager::load_users_locked() {
  const auto fail = [this](const std::string& reason) {
    std::cerr << "UserManager: failed to load user store '"
              << users_file.string() << "': " << reason << std::endl;
    return false;
  };

  if (users_file.empty())
    return fail("path is empty");

  std::string resolution_error;
  const auto storage_path = resolve_store_target(users_file, resolution_error);
  if (!storage_path)
    return fail(resolution_error);

  if (storage_path->lexically_normal() != users_file.lexically_normal())
    std::cout << "UserManager: following configured user store link to '"
              << storage_path->string() << "'" << std::endl;

  std::error_code ec;
  const auto file_status = std::filesystem::symlink_status(*storage_path, ec);
  if (ec)
    return fail("cannot inspect file type: " + ec.message());
  if (file_status.type() == std::filesystem::file_type::not_found) {
    users.clear();
    initialized = true;
    std::cout << "UserManager: user store not found; starting with an empty store"
              << std::endl;
    return true;
  }

  if (file_status.type() != std::filesystem::file_type::regular)
    return fail("path is not a regular file");

  std::ifstream file(*storage_path, std::ios::binary);
  if (!file)
    return fail("file could not be opened for reading");
  const std::string content{
      std::istreambuf_iterator<char>(file),
      std::istreambuf_iterator<char>()};
  if (!file.good() && !file.eof())
    return fail("file read failed");

  if (std::ranges::all_of(content, [](unsigned char value) {
        return std::isspace(value) != 0;
      })) {
    users.clear();
    initialized = true;
    std::cout << "UserManager: user store is empty; starting with an empty store"
              << std::endl;
    return true;
  }

  const auto data = crow::json::load(content);
  if (!data)
    return fail("JSON parsing failed");
  if (!data.has("users"))
    return fail("top-level field 'users' is missing");
  if (data["users"].t() != crow::json::type::List)
    return fail("top-level field 'users' must be an array");

  std::vector<User> loaded;
  std::set<std::string> usernames;
  std::size_t index = 0;
  try {
    for (const auto& item : data["users"]) {
      User user(item);
      std::string reason;
      if (!valid_user_record(user, reason))
        return fail("record[" + std::to_string(index) + "] (username '" +
                    user.username + "') is invalid: " + reason);
      if (!usernames.insert(user.username).second)
        return fail("record[" + std::to_string(index) +
                    "] duplicates username '" + user.username + "'");
      loaded.push_back(std::move(user));
      ++index;
    }
  } catch (const std::exception& error) {
    return fail("record[" + std::to_string(index) +
                "] could not be parsed: " + error.what());
  } catch (...) {
    return fail("record[" + std::to_string(index) +
                "] could not be parsed: unknown exception");
  }

  if (!loaded.empty() && std::ranges::none_of(
          loaded, [](const User& user) { return user.role == "admin"; }))
    return fail("at least one administrator account is required");

  users = std::move(loaded);
  initialized = true;
  std::cout << "UserManager: loaded " << users.size() << " users" << std::endl;
  return true;
}

bool UserManager::load_users() {
  std::lock_guard lock(users_mutex);
  if (!initialized && users_file.empty())
    return false;
  return load_users_locked();
}

bool UserManager::save_users() {
  std::lock_guard lock(users_mutex);
  if (!initialized)
    return false;
  return save_users_locked();
}

bool UserManager::save_users_locked() {
  if (users_file.empty()) {
    std::cerr << "UserManager: failed to save user store: path is empty"
              << std::endl;
    return false;
  }

  std::string resolution_error;
  const auto storage_path = resolve_store_target(users_file, resolution_error);
  if (!storage_path) {
    std::cerr << "UserManager: failed to save user store '"
              << users_file.string() << "': " << resolution_error << std::endl;
    return false;
  }

  crow::json::wvalue::list user_list;
  user_list.reserve(users.size());
  for (const auto& user : users)
    user_list.emplace_back(user.to_json());
  crow::json::wvalue root;
  root["users"] = std::move(user_list);
  const std::string serialized = root.dump();

  const std::filesystem::path parent =
      storage_path->parent_path().empty() ? std::filesystem::path(".")
                                          : storage_path->parent_path();
  const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path temporary =
      parent / (storage_path->filename().string() + ".tmp." +
                std::to_string(suffix));

  bool written = false;
  {
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (file) {
      file.write(
          serialized.data(),
          static_cast<std::streamsize>(serialized.size()));
      file.flush();
      written = static_cast<bool>(file);
    }
  }
  if (!written) {
    std::error_code cleanup_ec;
    std::filesystem::remove(temporary, cleanup_ec);
    std::cerr << "UserManager: failed to save user store '"
              << users_file.string() << "': temporary file could not be written"
              << std::endl;
    return false;
  }

  std::error_code ec;
  std::filesystem::rename(temporary, *storage_path, ec);
  if (ec) {
    std::filesystem::remove(temporary, ec);
    std::cerr << "UserManager: failed to save user store '"
              << users_file.string() << "': atomic rename failed: "
              << ec.message() << std::endl;
    return false;
  }
  return true;
}

} // namespace USER_MANAGER
