
#pragma once
#include <crow/http_response.h>
#include <crow/http_request.h>

namespace VIEWER{
crow::response info_renew(const crow::request&req);
} // namespace VIEWER