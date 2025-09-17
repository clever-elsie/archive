#pragma once
#include <crow/json.h>
#include <crow/http_request.h>

namespace VIEWER{
using namespace std;
crow::json::wvalue get_imgs(const crow::request&req);
crow::json::wvalue get_dir_list(const crow::request&req);
}// namespace comic