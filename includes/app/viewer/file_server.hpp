#pragma once
#include <crow/http_response.h>
#include <crow/http_request.h>

namespace VIEWER{
crow::response get_file_binary(const crow::request&req);
crow::response redirect_media(const crow::request&req);
} // namespace VIEWER