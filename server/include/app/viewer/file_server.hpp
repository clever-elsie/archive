#pragma once
#include <crow/http_response.h>
#include <crow/http_request.h>

namespace VIEWER{
crow::response redirect_media(const crow::request&req);
} // namespace VIEWER