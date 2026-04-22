#pragma once

#include <string>
#include <string_view>

#include <crow/http_request.h>

#include <manager/auth/middleware.hpp>
#include <manager/auth/auth.hpp>
#include <manager/users/manager.hpp>

#include <app/viewer/manager.hpp>

namespace VIEWER {

inline std::string norm_rel(std::string_view in) {
  std::string s(in);
  while (s.rfind("./", 0) == 0) s.erase(0, 2);
  while (!s.empty() && s.front() == '/') s.erase(s.begin());
  while (!s.empty() && s.back() == '/') s.pop_back();
  for (std::string::size_type i = 0; i + 1 < s.size();) {
    if (s[i] == '/' && s[i + 1] == '/') s.erase(i + 1, 1);
    else ++i;
  }
  return s;
}

inline bool is_admin_req(const crow::request& req) {
  const std::string token = MIDDLEWARE::extract_token(req);
  if (token.empty() || !AUTH::validate_token_wrapper(token)) return false;
  const std::string username = AUTH::get_username_from_token(token);
  if (username.empty()) return false;
  return USER_MANAGER::user_manager.is_admin(username);
}

inline bool is_public_dir_rel(std::string_view rel) {
  manager& mgr = manager::get_instance();
  const std::string key = norm_rel(rel);
  return mgr.public_dirs.contains(key);
}

inline bool can_view_node(const crow::request& req, const Info* node) {
  if (!node) return false;
  if (is_admin_req(req)) return true;
  // root は常に「遷移の起点」として許可（ただし中身は別途フィルタ）
  if (node == manager::get_root_dir()) return true;
  if(node->has_only_img()) return is_public_dir_rel(node->parent()->relative_path());
  return is_public_dir_rel(node->relative_path());
}

} // namespace VIEWER

