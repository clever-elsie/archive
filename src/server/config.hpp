#pragma once

#include "headers.hpp"
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <algorithm>

namespace CONFIG {
    struct Params {
        int SESSION_TIMEOUT_MINUTES;
        int SERVER_PORT;
        std::string SSL_CERT_PATH;
        std::string SSL_KEY_PATH;
        bool IS_DEVELOPMENT;
        std::vector<std::string> ALLOWED_ORIGINS;
    };

    // グローバル設定パラメータ
    inline Params params;

    // 設定ファイル(param.json)からパラメータを読み込む関数
    inline bool load_params(const std::string& filepath = "config/param.json") {
        std::ifstream ifs(filepath);
        if (!ifs) {
            std::cerr << "Failed to open config file: " << filepath << std::endl;
            return false;
        }
        try {
            std::string content((std::istreambuf_iterator<char>(ifs)),
                               std::istreambuf_iterator<char>());
            ifs.close();
            
            auto data = crow::json::load(content);
            if (!data) {
                std::cerr << "Failed to parse config file" << std::endl;
                return false;
            }
            
            params.SESSION_TIMEOUT_MINUTES = data["SESSION_TIMEOUT_MINUTES"].i();
            params.SERVER_PORT = data["SERVER_PORT"].i();
            params.SSL_CERT_PATH = data["SSL_CERT_PATH"].s();
            params.SSL_KEY_PATH = data["SSL_KEY_PATH"].s();
            params.IS_DEVELOPMENT = data["IS_DEVELOPMENT"].b();
            
            // ALLOWED_ORIGINS配列を読み込み
            params.ALLOWED_ORIGINS.clear();
            auto origins = data["ALLOWED_ORIGINS"];
            for (const auto& origin : origins) {
                params.ALLOWED_ORIGINS.push_back(origin.s());
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error parsing config: " << e.what() << std::endl;
            return false;
        }
        return true;
    }

    // 指定されたoriginが許可されているかチェック
    inline bool is_origin_allowed(const std::string& origin) {
        return std::find(params.ALLOWED_ORIGINS.begin(), params.ALLOWED_ORIGINS.end(), origin) != params.ALLOWED_ORIGINS.end();
    }

    // 許可されたoriginのリストを取得
    inline const std::vector<std::string>& get_allowed_origins() {
        return params.ALLOWED_ORIGINS;
    }

    // 許可されたoriginをカンマ区切りの文字列として取得（CORSヘッダー用）
    inline std::string get_origins_header() {
        std::string result;
        for (size_t i = 0; i < params.ALLOWED_ORIGINS.size(); ++i) {
            if (i > 0) result += ", ";
            result += params.ALLOWED_ORIGINS[i];
        }
        return result;
    }

    // CORS設定（ドメイン設定ファイルから動的に読み込み）
    inline const char* ALLOWED_METHODS = "GET, POST, OPTIONS";
    inline const char* ALLOWED_HEADERS = "Content-Type, X-Session-ID, X-CSRF-Token";
} 