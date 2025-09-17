#include <manager/users/manager.hpp>

namespace USER_MANAGER {
using namespace std;

User::User(const crow::json::rvalue&json) {
  username = json["username"].s();
  password_hash = json["password_hash"].s();
  if (json.has("password_salt"))
    password_salt = json["password_salt"].s();
  else password_salt = "";
  if(json.has("password_iter"))
    password_iter = json["password_iter"].i();
  else password_iter = 0;
  role = json["role"].s();
  created_by = json["created_by"].s();
  created_at = json["created_at"].s();
  last_login = json["last_login"].s();
}

crow::json::wvalue User::to_json()const {
  crow::json::wvalue json;
  json["username"] = username;
  json["password_hash"] = password_hash;
  json["password_salt"] = password_salt;
  json["password_iter"] = password_iter;
  json["role"] = role;
  json["created_by"] = created_by;
  json["created_at"] = created_at;
  json["last_login"] = last_login;
  return json;
}
}