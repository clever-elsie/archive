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

    // パス解決（サンドボックス: base_dir 配下）
    if(!(type=="image"||type=="video"||type=="audio"||type=="text"))
        return crow::response(400, "Unknown type");

    std::string base;
    {
        lock_guard<mutex> lock(mgr.imtex);
        Info* node=mgr.get_info_from_id(idv);
        if(!mgr.is_valid(node)||!node->refresh(0)) return crow::response(404);
        if(type=="image" && node->imgs.empty()) return crow::response(404);
        base = node->path;
    }

    std::filesystem::path fullpath = std::filesystem::path(base) / filename;
    std::error_code ec;
    auto canon_base = std::filesystem::weakly_canonical(std::filesystem::path(mgr.base_dir), ec);
    auto canon_fp   = std::filesystem::weakly_canonical(fullpath, ec);
    if(ec || canon_fp.empty() || canon_base.empty()) return crow::response(404);
    // base_dir 配下にあるかチェック
    auto canon_base_str = canon_base.generic_string();
    auto canon_fp_str   = canon_fp.generic_string();
    if(canon_fp_str.rfind(canon_base_str, 0) != 0) return crow::response(403);

    // 内部URI（NGINX 側で location /_protected_media/ { internal; alias <base_dir>/; } を設定）
    std::filesystem::path rel = std::filesystem::relative(canon_fp, canon_base, ec);
    if(ec) return crow::response(404);
    std::string internal_uri = std::string("/_protected_media/") + rel.generic_string();

    crow::response res;
    // Content-Type（任意: NGINX 側でも判定可能）
    if(type=="image"){
        if(filename.ends_with(".jpg")||filename.ends_with(".jpeg")) res.set_header("Content-Type","image/jpeg");
        else if(filename.ends_with(".png")) res.set_header("Content-Type","image/png");
        else if(filename.ends_with(".webp")) res.set_header("Content-Type","image/webp");
        else if(filename.ends_with(".gif")) res.set_header("Content-Type","image/gif");
        else res.set_header("Content-Type","application/octet-stream");
    }else if(type=="video"){
        if(filename.ends_with(".mp4")) res.set_header("Content-Type","video/mp4");
        else res.set_header("Content-Type","application/octet-stream");
    }else if(type=="audio"){
        if(filename.ends_with(".mp3")) res.set_header("Content-Type","audio/mpeg");
        else if(filename.ends_with(".flac")) res.set_header("Content-Type","audio/flac");
        else if(filename.ends_with(".aac")) res.set_header("Content-Type","audio/aac");
        else if(filename.ends_with(".wav")) res.set_header("Content-Type","audio/wav");
        else res.set_header("Content-Type","application/octet-stream");
    }else if(type=="text"){
        res.set_header("Content-Type","text/plain; charset=utf-8");
    }
    res.set_header("Accept-Ranges","bytes");
    res.set_header("X-Accel-Redirect", internal_uri);
    res.set_header("X-Content-Type-Options", "nosniff");
    res.code = 200;
    return res;
}
} // namespace VIEWER