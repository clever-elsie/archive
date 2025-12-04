#include <fstream>
#include <iostream>

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
    vector<string> allowed_origins;
    for (const auto& origin : data["ALLOWED_ORIGINS"])
      allowed_origins.push_back(origin.s());
    params = CONFIG::Params(
      data["SESSION_TIMEOUT_MINUTES"].i(),
      data["SERVER_PORT"].i(),
      data["SSL_CERT_PATH"].s(),
      data["SSL_KEY_PATH"].s(),
      data["JWT_SECRET_KEY"].s(),
      data["IS_DEVELOPMENT"].b(),
      move(allowed_origins),
      data["ALLOWED_METHODS"].s(),
      data["ALLOWED_HEADERS"].s(),
      data["VIEWER_DIR"].s()
    );
    
  } catch (const exception& e) {
    cerr << "Error parsing config: " << e.what() << endl;
    return false;
  }
  // ドメイン設定を初期化
  cout << "=== Domain Configuration ===" << endl;
  cout << "Config file: config/param.json" << endl;
  cout << "Allowed origins:" << endl;
  for (const auto& origin : CONFIG::get_allowed_origins()) {
    cout << "  - " << origin << endl;
  }
  cout << "===========================" << endl;
  return true;
}
} // namespace CONFIG