#pragma once
#include <crow/http_request.h>

namespace VIEWER{
crow::response retrieve_query(const crow::request& req);
} // namespace VIEWER