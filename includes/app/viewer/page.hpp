#pragma once
#include <crow/http_request.h>
#include <crow/json.h>

namespace VIEWER{

crow::json::wvalue get_page_data(const crow::request&req);

} // namespace VIEWER