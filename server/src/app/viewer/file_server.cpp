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

  // パス解決（サンドボックス: base_dir 配下）
  if(!(type=="image"||type=="video"||type=="audio"||type=="text"||type=="doc"))
    return crow::response(400, "Unknown type");

  std::filesystem::path rel;
  try{
    rel = node->locate_media(Info::classify(filename), filename);
  }catch(int e){ return crow::response(e); }

  std::string internal_uri = std::string("/_protected_media/") + rel.generic_string();

  crow::response res;
  res.set_header("Accept-Ranges","bytes");
  res.set_header("X-Accel-Redirect", internal_uri);
  res.set_header("X-Content-Type-Options", "nosniff");
  res.code = 200;
  return res;
}
} // namespace VIEWER