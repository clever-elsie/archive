#pragma once
#include <crow/http_response.h>
#include <crow/http_request.h>

namespace VIEWER{
using namespace std;

void load_leaf_dir(const string&base);
crow::response reload_leaf(const crow::request&req);

} // namespace VIEWER