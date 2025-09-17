#pragma once
#include <crow.h>
#include <manager/auth/middleware.hpp>

namespace VIEWER{

void setup(crow::App<MIDDLEWARE::AuthMiddleware>& app, std::string&& viewer_dir);

} // namespace VIEWER