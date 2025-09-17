#pragma once
#include <crow.h>

#include "auth.hpp"
#include "middleware.hpp"

namespace AUTH{

void setup_auth_routes(crow::App<MIDDLEWARE::AuthMiddleware>& app);

} // namespace AUTH