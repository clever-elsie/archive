#pragma once
#include <crow/json.h>

namespace VIEWER{
crow::json::wvalue get_rand_imgs(int cnt);
} // namespace VIEWER