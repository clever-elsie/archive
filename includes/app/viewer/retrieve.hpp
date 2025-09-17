#pragma once
#include <crow/json.h>

#include <app/viewer/global.hpp>

namespace VIEWER{
crow::json::wvalue retrieve_query(const crow::request& req);
} // namespace VIEWER