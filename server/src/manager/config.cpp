#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include <crow/json.h>
#include <manager/config.hpp>

namespace CONFIG{
using namespace std;
using isit=istreambuf_iterator<char>;

bool load_params(const string& filepath) {
  ifstream ifs(filepath);
  if (!ifs) {
    cerr << "Failed to open config file: " << filepath << endl;
    return false;
  }
  try {
    string content((isit(ifs)), isit());
    ifs.close();
    
    auto data = crow::json::load(content);
    if (!data) {
      cerr << "Failed to parse config file" << endl;
      return false;
    }

    const auto required_string = [&data](const char* name) {
      if (!data.has(name) ||
          data[name].t() != crow::json::type::String)
        throw runtime_error(string(name) + " must be a non-empty string");
      const string value = data[name].s();
      if (value.empty())
        throw runtime_error(string(name) + " must be a non-empty string");
      return value;
    };
    const auto required_int = [&data](const char* name) {
      if (!data.has(name) || data[name].t() != crow::json::type::Number)
        throw runtime_error(string(name) + " must be a number");
      return data[name].i();
    };

    const int session_timeout_minutes =
        required_int("SESSION_TIMEOUT_MINUTES");
    if (session_timeout_minutes <= 0)
      throw runtime_error("SESSION_TIMEOUT_MINUTES must be positive");
    const int server_port = required_int("SERVER_PORT");
    if (server_port < 1 || server_port > 65535)
      throw runtime_error("SERVER_PORT must be between 1 and 65535");
    const string ssl_cert_path = required_string("SSL_CERT_PATH");
    const string ssl_key_path = required_string("SSL_KEY_PATH");
    const string jwt_secret_key = required_string("JWT_SECRET_KEY");
    if (jwt_secret_key.size() < 32)
      throw runtime_error("JWT_SECRET_KEY must contain at least 32 bytes");
    const string allowed_methods = required_string("ALLOWED_METHODS");
    const string allowed_headers = required_string("ALLOWED_HEADERS");
    const string viewer_dir = required_string("VIEWER_DIR");
    if (!data.has("ALLOWED_ORIGINS") ||
        data["ALLOWED_ORIGINS"].t() != crow::json::type::List)
      throw runtime_error("ALLOWED_ORIGINS must be a list");
    vector<string> allowed_origins;
    for (const auto& origin : data["ALLOWED_ORIGINS"]) {
      if (origin.t() != crow::json::type::String)
        throw runtime_error("ALLOWED_ORIGINS contains an invalid origin");
      const string value = origin.s();
      if (value.empty() || value == "*")
        throw runtime_error("ALLOWED_ORIGINS contains an invalid origin");
      allowed_origins.push_back(value);
    }
    vector<string> viewer_pub_list;
    if (data.has("VIEWER_PUB_LIST")) {
      if (data["VIEWER_PUB_LIST"].t() != crow::json::type::List)
        throw runtime_error("VIEWER_PUB_LIST must be a list");
      for (const auto& p : data["VIEWER_PUB_LIST"]) {
        if (p.t() != crow::json::type::String)
          throw runtime_error("VIEWER_PUB_LIST contains an invalid path");
        viewer_pub_list.push_back(p.s());
      }
    }
    string user_store_path = "users.json";
    if (data.has("USER_STORE_PATH")) {
      if (data["USER_STORE_PATH"].t() != crow::json::type::String)
        throw runtime_error("USER_STORE_PATH must be a string");
      user_store_path = data["USER_STORE_PATH"].s();
      if (user_store_path.empty())
        throw runtime_error("USER_STORE_PATH must not be empty");
    }
    constexpr int default_viewer_scan_interval_seconds = 3 * 60 * 60;
    int viewer_scan_interval_seconds = default_viewer_scan_interval_seconds;
    if (data.has("VIEWER_SCAN_INTERVAL_SECONDS")) {
      if (data["VIEWER_SCAN_INTERVAL_SECONDS"].t() != crow::json::type::Number)
        throw runtime_error("VIEWER_SCAN_INTERVAL_SECONDS must be a number");
      viewer_scan_interval_seconds = data["VIEWER_SCAN_INTERVAL_SECONDS"].i();
      if (viewer_scan_interval_seconds <= 0)
        throw runtime_error("VIEWER_SCAN_INTERVAL_SECONDS must be positive");
    }
    if (!data.has("IS_DEVELOPMENT") ||
        (data["IS_DEVELOPMENT"].t() != crow::json::type::True &&
         data["IS_DEVELOPMENT"].t() != crow::json::type::False))
      throw runtime_error("IS_DEVELOPMENT must be boolean");
    params = CONFIG::Params(
      session_timeout_minutes,
      server_port,
      ssl_cert_path,
      ssl_key_path,
      jwt_secret_key,
      data["IS_DEVELOPMENT"].b(),
      move(allowed_origins),
      allowed_methods,
      allowed_headers,
      viewer_dir,
      move(viewer_pub_list),
      move(user_store_path)
    );
    params.VIEWER_SCAN_INTERVAL_SECONDS = viewer_scan_interval_seconds;
    
  } catch (const exception& e) {
    cerr << "Error parsing config: " << e.what() << endl;
    return false;
  }
  // ドメイン設定を初期化
  cout << "=== Domain Configuration ===" << endl;
  cout << "Config file: " << filepath << endl;
  cout << "Allowed origins:" << endl;
  for (const auto& origin : CONFIG::get_allowed_origins()) {
    cout << "  - " << origin << endl;
  }
  cout << "===========================" << endl;
  return true;
}

string config_path_from_args(int argc, char* argv[]) {
  string path = "config/param.json";
  bool positional_seen = false;
  bool explicit_option_seen = false;
  for (int i = 1; i < argc; ++i) {
    const string_view arg = argv[i] ? string_view(argv[i]) : string_view();
    if (arg == "--config" || arg == "-c") {
      if (i + 1 >= argc || argv[i + 1] == nullptr)
        return {};
      path = argv[++i];
      explicit_option_seen = true;
      continue;
    }
    if (!arg.empty() && arg.front() != '-' && !positional_seen &&
        !explicit_option_seen) {
      path = string(arg);
      positional_seen = true;
      continue;
    }
    if (arg == "--help" || arg == "-h")
      return {};
  }
  return path;
}
} // namespace CONFIG
