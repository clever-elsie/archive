#include <mutex>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <system_error>

#include <crow/json.h>

#include <app/viewer/inline_helper.hpp>
#include <app/viewer/file_server.hpp>
#include <app/viewer/manager.hpp>
#include <app/viewer/access_control.hpp>
#include <app/viewer/zip_util.hpp>

namespace VIEWER{
using namespace std;

// NGINX に配信をオフロード（X-Accel-Redirect）するためのストリーミング用エンドポイント
// GET /req/media?type=video|audio|image|text&id=<dir_or_leaf_id>&filename=<name>&token=<jwt>
crow::response redirect_media(const crow::request& req){
  const char* type_c = req.url_params.get("type");
  const char* id_c   = req.url_params.get("id");
  const char* fn_c   = req.url_params.get("filename");
  if(!type_c || !id_c || !fn_c) return crow::response(400, "missing query");

  manager& mgr = manager::get_instance();
  string type(type_c);
  string filename(fn_c);
  uint64_t idv;
  try{ idv = static_cast<uint64_t>(std::stoull(id_c)); }catch(...){ return crow::response(400, "bad id"); }
  Info* node=mgr.get_info_from_id(idv);
  if(!node || !mgr.is_valid(node)) return crow::response(404, "not found");

  // パス解決（サンドボックス: base_dir 配下）
  if(!(type=="image"||type=="video"||type=="audio"||type=="text"||type=="doc"))
    return crow::response(400, "Unknown type");

  if(!VIEWER::is_admin_req(req) && !VIEWER::can_view_node(req, node))
    return crow::response(403, "forbidden");

  std::filesystem::path rel;
  try{
    rel = node->locate_media(Info::classify(filename), filename);
  }catch(int e){ return crow::response(e); }

  std::filesystem::path full_p = std::filesystem::path(mgr.base_dir) / rel;

  // zipファイルの場合は、1枚目の画像バイナリを直接抽出してレスポンス送信（raw=1 が指定された場合はZIP本体を返却）
  if(rel.extension() == ".zip" && std::filesystem::is_regular_file(full_p)) {
    const char* raw_c = req.url_params.get("raw");
    bool is_raw = raw_c && (std::string(raw_c) == "1" || std::string(raw_c) == "true");

    if (!is_raw) {
      auto extracted = zip_util::extract_first_image(full_p);
      if(extracted) {
        crow::response res(200, extracted->mime_type, std::string(extracted->data.begin(), extracted->data.end()));
        res.set_header("Cache-Control", "public, max-age=86400");
        res.set_header("X-Content-Type-Options", "nosniff");
        return res;
      }
    }
  }

  std::string internal_uri = std::string("/_protected_media/") + rel.generic_string();

  crow::response res;
  res.set_header("Accept-Ranges","bytes");
  res.set_header("X-Accel-Redirect", internal_uri);
  res.set_header("X-Content-Type-Options", "nosniff");
  res.code = 200;
  return res;
}
} // namespace VIEWER