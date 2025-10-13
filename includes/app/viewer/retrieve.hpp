#pragma once
#include <crow/json.h>
#include <crow/http_request.h>

namespace VIEWER{
crow::json::wvalue retrieve_query(const crow::request& req);
} // namespace VIEWER