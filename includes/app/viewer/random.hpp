#pragma once
#include <crow/json.h>

#include <app/viewer/global.hpp>

namespace VIEWER{
crow::json::wvalue get_rand_imgs();
} // namespace VIEWER