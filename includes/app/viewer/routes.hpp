#pragma once
#include <crow.h>
#include "viewer.hpp"
#include <manager/auth/middleware.hpp>

namespace VIEWER_ROUTES{

void setup_viewer_routes(crow::App<MIDDLEWARE::AuthMiddleware>& app, std::string&& viewer_dir);

} // namespace VIEWER_ROUTES