#pragma once
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <openssl/evp.h>
#include <fstream>
#include <algorithm>
#include <iostream>
#include "../../headers.hpp"
#include <thread>
#include <atomic>
#include <signal.h>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <ctime>
#include "crow.h"
#include <mutex>

namespace USER_MANAGER {

struct User {
    std::string username;
    std::string password_hash;
    std::string role; // "admin" or "user"
    std::string created_by;
    std::string created_at;
    std::string last_login;
};

class UserManager {
private:
    std::vector<User> users;
    std::string users_file = "users.json";

    std::mutex users_mutex;
    
    // シグナルハンドラー用のグローバルインスタンスポインタ
    static UserManager* instance;
    
    inline std::string hash_password(const std::string& password) {
        EVP_MD_CTX* context = EVP_MD_CTX_new();
        if (context == nullptr) {
            return "";
        }
        
        if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) != 1) {
            EVP_MD_CTX_free(context);
            return "";
        }
        
        if (EVP_DigestUpdate(context, password.c_str(), password.length()) != 1) {
            EVP_MD_CTX_free(context);
            return "";
        }
        
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int lengthOfHash = 0;
        
        if (EVP_DigestFinal_ex(context, hash, &lengthOfHash) != 1) {
            EVP_MD_CTX_free(context);
            return "";
        }
        
        EVP_MD_CTX_free(context);
        return bytes_to_hex(hash, lengthOfHash);
    }
    
    inline std::string bytes_to_hex(const unsigned char* data, size_t len) {
        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < len; ++i) {
            ss << std::setw(2) << static_cast<int>(data[i]);
        }
        return ss.str();
    }
    
    inline std::string get_current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    // シグナルハンドラー


public:
    UserManager() {
        // グローバルインスタンスポインタを設定
        instance = this;
        
        // ユーザーデータを読み込み
        load_users();
        
        std::cout << "UserManager: Initialized" << std::endl;
    }
    
    ~UserManager() {
        instance = nullptr;
    }
    
    // ファイル操作
    inline bool load_users() {
        std::lock_guard<std::mutex> lock(users_mutex);
        std::ifstream file(users_file);
        if (!file.is_open()) {
            std::cout << "UserManager: users.json not found, starting with empty user list" << std::endl;
            return false; // ファイルが存在しない場合は正常（初回起動）
        }
        
        try {
            std::string content((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            file.close();
            
            auto data = crow::json::load(content);
            if (!data) {
                std::cout << "UserManager: Failed to parse users.json" << std::endl;
                return false;
            }
            
            users.clear();
            
            // JSON構造を正しく解析: {"users": [...]}
            if (data.has("users")) {
                auto users_array = data["users"];
                for (const auto& user_data : users_array) {
                    User user;
                    user.username = user_data["username"].s();
                    user.password_hash = user_data["password_hash"].s();
                    user.role = user_data["role"].s();
                    user.created_by = user_data["created_by"].s();
                    user.created_at = user_data["created_at"].s();
                    user.last_login = user_data["last_login"].s();
                    users.push_back(user);
                }
                std::cout << "UserManager: Loaded " << users.size() << " users from users.json" << std::endl;
            } else {
                std::cout << "UserManager: No 'users' key found in users.json" << std::endl;
            }
            return true;
        } catch (const std::exception& e) {
            std::cout << "UserManager: Exception while loading users: " << e.what() << std::endl;
            return false;
        }
    }
    
    inline bool save_users() {
        try {
            crow::json::wvalue::list user_list;
            for (const auto& user : users) {
                crow::json::wvalue user_data;
                user_data["username"] = user.username;
                user_data["password_hash"] = user.password_hash;
                user_data["role"] = user.role;
                user_data["created_by"] = user.created_by;
                user_data["created_at"] = user.created_at;
                user_data["last_login"] = user.last_login;
                user_list.emplace_back(std::move(user_data));
            }
            
            crow::json::wvalue root;
            root["users"] = std::move(user_list);
            
            std::ofstream file(users_file);
            if (!file.is_open()) {
                std::cout << "UserManager: Failed to open users.json for writing" << std::endl;
                return false;
            }
            
            file << root.dump();
            file.close();
            std::cout << "UserManager: Saved " << users.size() << " users to users.json" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cout << "UserManager: Exception while saving users: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 内部用の保存メソッド（mutex付き）
    inline bool save_users_locked() {
        std::lock_guard<std::mutex> lock(users_mutex);
        return save_users();
    }
    
    // ユーザー操作
    inline bool add_user(const std::string& username, const std::string& password, 
                      const std::string& role, const std::string& created_by) {
        if (user_exists(username)) {
            return false; // ユーザーが既に存在
        }
        
        User new_user;
        new_user.username = username;
        new_user.password_hash = hash_password(password);
        new_user.role = role;
        new_user.created_by = created_by;
        new_user.created_at = get_current_timestamp();
        new_user.last_login = "";
        
        std::lock_guard<std::mutex> lock(users_mutex);
        users.push_back(new_user);
        std::cout << "UserManager: Added user '" << username << "' with role '" << role << "'" << std::endl;
        return save_users(); // 即座に保存
    }
    
    inline bool delete_user(const std::string& username, const std::string& deleted_by) {
        if (!can_manage_users(deleted_by)) {
            return false; // 権限なし
        }
        
        std::lock_guard<std::mutex> lock(users_mutex);
        auto it = std::find_if(users.begin(), users.end(),
                              [&username](const User& user) { return user.username == username; });
        
        if (it == users.end()) {
            return false; // ユーザーが存在しない
        }
        
        if (it->role == "admin" && !is_admin(deleted_by)) {
            return false; // 一般ユーザーは管理者を削除できない
        }
        
        users.erase(it);
        std::cout << "UserManager: Deleted user '" << username << "' by '" << deleted_by << "'" << std::endl;
        return save_users(); // 即座に保存
    }
    
    inline bool promote_user(const std::string& username, const std::string& promoted_by) {
        if (!is_admin(promoted_by)) {
            return false; // 管理者のみ昇格可能
        }
        
        std::lock_guard<std::mutex> lock(users_mutex);
        auto it = std::find_if(users.begin(), users.end(),
                              [&username](const User& user) { return user.username == username; });
        
        if (it == users.end()) {
            return false;
        }
        
        it->role = "admin";
        std::cout << "UserManager: Promoted user '" << username << "' to admin by '" << promoted_by << "'" << std::endl;
        return save_users(); // 即座に保存
    }
    
    inline bool demote_user(const std::string& username, const std::string& demoted_by) {
        if (!is_admin(demoted_by)) {
            return false; // 管理者のみ降格可能
        }
        
        std::lock_guard<std::mutex> lock(users_mutex);
        auto it = std::find_if(users.begin(), users.end(),
                              [&username](const User& user) { return user.username == username; });
        
        if (it == users.end()) {
            return false;
        }
        
        it->role = "user";
        std::cout << "UserManager: Demoted user '" << username << "' to user by '" << demoted_by << "'" << std::endl;
        return save_users(); // 即座に保存
    }
    
    // 認証・権限チェック
    inline bool authenticate_user(const std::string& username, const std::string& password) {
        std::lock_guard<std::mutex> lock(users_mutex);
        auto it = std::find_if(users.begin(), users.end(),
                              [&username](const User& user) { return user.username == username; });
        
        if (it == users.end()) {
            return false;
        }
        
        std::string input_hash = hash_password(password);
        if (it->password_hash == input_hash) {
            update_last_login(username);
            return true;
        }
        
        return false;
    }
    
    inline bool is_admin(const std::string& username) {
        std::lock_guard<std::mutex> lock(users_mutex);
        auto it = std::find_if(users.begin(), users.end(),
                              [&username](const User& user) { return user.username == username; });
        
        return (it != users.end() && it->role == "admin");
    }
    
    // ユーザー情報取得
    inline std::vector<User> get_all_users() {
        std::lock_guard<std::mutex> lock(users_mutex);
        return users;
    }
    
    inline std::string get_user_role(const std::string& username) {
        std::lock_guard<std::mutex> lock(users_mutex);
        auto it = std::find_if(users.begin(), users.end(),
                              [&username](const User& user) { return user.username == username; });
        
        if (it != users.end()) {
            return it->role;
        }
        return "";
    }
    
    inline void update_last_login(const std::string& username) {
        auto it = std::find_if(users.begin(), users.end(),
                              [&username](const User& user) { return user.username == username; });
        
        if (it != users.end()) {
            it->last_login = get_current_timestamp();
            save_users(); // 即座に保存
        }
    }
    
    // 内部用の最終ログイン更新メソッド（mutex付き）
    inline void update_last_login_locked(const std::string& username) {
        std::lock_guard<std::mutex> lock(users_mutex);
        update_last_login(username);
    }
    
    // 権限チェック
    inline bool can_register_admin(const std::string& username) {
        return is_first_user();
    }
    
    inline bool can_register_user(const std::string& username) {
        return is_admin(username);
    }
    
    inline bool is_first_user() {
        std::lock_guard<std::mutex> lock(users_mutex);
        return users.empty();
    }
    
    inline bool user_exists(const std::string& username) {
        std::lock_guard<std::mutex> lock(users_mutex);
        auto it = std::find_if(users.begin(), users.end(),
                              [&username](const User& user) { return user.username == username; });
        return it != users.end();
    }
    
    inline bool can_manage_users(const std::string& username) {
        return is_admin(username);
    }
};

// 静的メンバー変数の定義
UserManager* UserManager::instance = nullptr;

// グローバルインスタンス
inline UserManager user_manager;

} // namespace USER_MANAGER 