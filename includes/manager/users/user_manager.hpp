#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <iostream>
#include <mutex>

#include <crow/json.h>

namespace USER_MANAGER {

struct User {
  std::string username;
  std::string password_hash;
  std::string password_salt; // hex encoded
  int password_iter; // PBKDF2 iterations (0 for legacy)
  std::string role; // "admin" or "user"
  std::string created_by;
  std::string created_at;
  std::string last_login;
  User()=default;
  User(const crow::json::rvalue&json);
  crow::json::wvalue to_json()const;
};

class UserManager { // Singleton
private:
  static constexpr int DEFAULT_PBKDF2_ITERATIONS = 200000;
  std::vector<User> users;
  std::string users_file = "users.json";
  std::mutex users_mutex;
  // シグナルハンドラー用のグローバルインスタンスポインタ
  static UserManager* instance;
 
  // Legacy SHA-256 (no salt) - keep for backward compatibility and migration
  std::string hash_password_legacy_sha256(const std::string& password)const;
  std::vector<unsigned char> hex_to_bytes(const std::string& hex)const;

  std::string generate_salt_hex(size_t num_bytes = 16)const;
  std::string hash_password_pbkdf2_sha256(const std::string& password, const std::string& salt_hex, int iterations)const;
  std::string bytes_to_hex(const unsigned char* data, size_t len)const;
  std::string get_current_timestamp()const;

public:
  UserManager() {
    instance = this; // グローバルインスタンスポインタを設定
    load_users(); // ユーザーデータを読み込み
    std::cout << "UserManager: Initialized" << std::endl;
  }
  ~UserManager() { instance = nullptr; }
  
  // ファイル操作
  bool load_users();
  bool save_users();
  bool save_users_locked(); // 内部用の保存メソッド（mutex付き）
  
  // ユーザー操作
  bool add_user(const std::string& username, const std::string& password, const std::string& role, const std::string& created_by);
  bool delete_user(const std::string& username, const std::string& deleted_by);
  private:
  bool change_user_role(const std::string&username, const char*const role, const std::string& changed_by);
  public:
  bool promote_user(const std::string& username, const std::string& promoted_by);
  bool demote_user(const std::string& username, const std::string& demoted_by);
  
  // 認証・権限チェック
  bool authenticate_user(const std::string& username, const std::string& password);
  bool is_admin(const std::string& username);
  // 権限チェック
  bool can_register_admin(const std::string& username);
  bool can_register_user(const std::string& username);
  bool can_manage_users(const std::string& username);
  bool is_first_user();
  bool user_exists(const std::string& username);
  
  // ユーザー情報取得
  std::vector<User> get_all_users();
  std::string get_user_role(const std::string& username);
  void update_last_login(const std::string& username);
  // 内部用の最終ログイン更新メソッド（mutex付き）
  void update_last_login_locked(const std::string& username);
  
};

inline UserManager* UserManager::instance=nullptr;

// グローバルインスタンス
inline UserManager user_manager;

} // namespace USER_MANAGER 