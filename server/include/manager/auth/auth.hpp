#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <crow/http_request.h>
#include <crow/http_response.h>

#include <manager/auth/jwt.hpp>

namespace AUTH {

struct Principal {
  std::string username;
  std::string role;
  std::uint64_t session_generation = 0;

  bool is_admin() const { return role == "admin"; }
};

std::string generate_token(const std::string& username);
bool validate_token(const std::string& token);
std::string get_username_from_token(const std::string& token);
bool authenticate_user(
    const std::string& username,
    const std::string& password);
std::optional<Principal> principal_from_token(const std::string& token);
std::optional<Principal> principal_from_request(const crow::request& req);

inline bool validate_token_wrapper(const std::string& token) {
  return validate_token(token);
}

inline std::string create_token(const std::string& username) {
  return generate_token(username);
}

std::string extract_cookie(
    const crow::request& req,
    std::string_view name);
std::string create_csrf_token();
bool validate_csrf_token(const crow::request& req);
void add_csrf_cookie(crow::response& response, const std::string& token);
void clear_auth_cookies(crow::response& response);

crow::response login_response(const crow::request& req);
crow::response logout_response(const crow::request& req);
crow::response check_auth_response(const crow::request& req);

} // namespace AUTH
