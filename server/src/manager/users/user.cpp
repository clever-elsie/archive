#include <manager/users/manager.hpp>

#include <limits>
#include <stdexcept>

namespace USER_MANAGER {

User::User(const crow::json::rvalue& json) {
  if (!json)
    throw std::runtime_error("user record is not a JSON object");

  const auto required = [&json](const char* name) {
    if (!json.has(name))
      throw std::runtime_error(
          std::string("user record is missing field '") + name + "'");
  };
  for (const char* name : {"username", "password_hash", "password_salt",
                           "password_iter", "role", "created_by",
                           "created_at", "last_login"})
    required(name);

  const auto required_string = [&json](const char* name) {
    if (json[name].t() != crow::json::type::String)
      throw std::runtime_error(
          std::string("user record field '") + name +
          "' must be a string");
  };
  for (const char* name : {"username", "password_hash", "password_salt",
                           "role", "created_by", "created_at", "last_login"})
    required_string(name);
  if (json["password_iter"].t() != crow::json::type::Number ||
      (json["password_iter"].nt() != crow::json::num_type::Signed_integer &&
       json["password_iter"].nt() != crow::json::num_type::Unsigned_integer))
    throw std::runtime_error("user record contains an invalid iteration count");
  username = json["username"].s();
  password_hash = json["password_hash"].s();
  password_salt = json["password_salt"].s();
  if (json["password_iter"].nt() == crow::json::num_type::Signed_integer) {
    const auto value = json["password_iter"].i();
    if (value < 0 || value > std::numeric_limits<int>::max())
      throw std::runtime_error("user record contains an invalid iteration count");
    password_iter = static_cast<int>(value);
  } else {
    const auto value = json["password_iter"].u();
    if (value > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
      throw std::runtime_error("user record contains an invalid iteration count");
    password_iter = static_cast<int>(value);
  }
  role = json["role"].s();
  created_by = json["created_by"].s();
  created_at = json["created_at"].s();
  last_login = json["last_login"].s();
  if (json.has("session_generation")) {
    if (json["session_generation"].t() != crow::json::type::Number ||
        (json["session_generation"].nt() != crow::json::num_type::Signed_integer &&
         json["session_generation"].nt() != crow::json::num_type::Unsigned_integer))
      throw std::runtime_error("user record contains an invalid session generation");
    if (json["session_generation"].nt() == crow::json::num_type::Signed_integer) {
      const auto value = json["session_generation"].i();
      if (value < 0)
        throw std::runtime_error("user record contains an invalid session generation");
      session_generation = static_cast<std::uint64_t>(value);
    } else {
      session_generation = json["session_generation"].u();
    }
  }
}

crow::json::wvalue User::to_json() const {
  crow::json::wvalue json;
  json["username"] = username;
  json["password_hash"] = password_hash;
  json["password_salt"] = password_salt;
  json["password_iter"] = password_iter;
  json["role"] = role;
  json["created_by"] = created_by;
  json["created_at"] = created_at;
  json["last_login"] = last_login;
  json["session_generation"] = session_generation;
  return json;
}

} // namespace USER_MANAGER
