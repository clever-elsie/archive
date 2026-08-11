#pragma once

#include <string>

#include <crow.h>

#include <manager/auth/middleware.hpp>
#include <manager/auth/authorization_middleware.hpp>

namespace VIEWER {

using App = crow::App<MIDDLEWARE::AuthMiddleware, MIDDLEWARE::AuthorizationMiddleware>;

void setup(App& app, std::string&& viewer_dir);

} // namespace VIEWER
