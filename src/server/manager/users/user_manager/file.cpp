#include "manager/users/user_manager.hpp"
#include <fstream>
#include <exception>

namespace USER_MANAGER {
using namespace std;
using isit=istreambuf_iterator<char>;

bool UserManager::load_users() {
  lock_guard<mutex> lock(users_mutex);
  ifstream file(users_file);
  if (!file.is_open()) {
    cout << "UserManager: users.json not found, starting with empty user list" << endl;
    return false; // ファイルが存在しない場合は正常（初回起動）
  }
  
  try {
    string content((isit(file)), isit());
    file.close();
    auto data = crow::json::load(content);
    if (!data) {
      cout << "UserManager: Failed to parse users.json" << endl;
      return false;
    }
    users.clear();
    if (data.has("users")) {
      auto users_array = data["users"];
      for (const auto& user_data : users_array)
        users.push_back(User(user_data));
      cout << "UserManager: Loaded " << users.size() << " users from users.json" << endl;
    } else cout << "UserManager: No 'users' key found in users.json" << endl;
    return true;
  } catch (const exception& e) {
    cout << "UserManager: Exception while loading users: " << e.what() << endl;
    return false;
  }
}

bool UserManager::save_users() {
  try {
    crow::json::wvalue::list user_list;
    for (const auto& user : users)
      user_list.emplace_back(user.to_json());
    crow::json::wvalue root;
    root["users"] = move(user_list);
    ofstream file(users_file);
    if (!file.is_open()) {
      cout << "UserManager: Failed to open users.json for writing" << endl;
      return false;
    }
    file << root.dump();
    file.close();
    cout << "UserManager: Saved " << users.size() << " users to users.json" << endl;
    return true;
  } catch (const exception& e) {
    cout << "UserManager: Exception while saving users: " << e.what() << endl;
    return false;
  }
}

bool UserManager::save_users_locked() {
  lock_guard<mutex> lock(users_mutex);
  return save_users();
}
} // namespace USER_MANAGER