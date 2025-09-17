#pragma once
#include <crow.h>

#include "manager.hpp"
#include "api.hpp"
#include <manager/auth/middleware.hpp>


namespace USER_ROUTES {

void setup_user_routes(crow::App<MIDDLEWARE::AuthMiddleware>& app);

} // namespace USER_ROUTES 