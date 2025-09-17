#pragma once
#include <crow/http_response.h>
#include <crow/http_request.h>

namespace VIEWER{

crow::json::wvalue get_page_list(const crow::request&req);
crow::json::wvalue get_page(const crow::request&req);

} // namespace VIEWER