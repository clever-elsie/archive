#pragma once
#include <crow/json.h>

namespace VIEWER{
crow::json::wvalue get_rand_imgs(const crow::request& req, int cnt);
} // namespace VIEWER