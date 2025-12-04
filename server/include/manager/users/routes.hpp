#pragma once
#include <crow.h>

#include "manager.hpp"
#include "api.hpp"
#include <manager/auth/middleware.hpp>


namespace USER {

void setup(crow::App<MIDDLEWARE::AuthMiddleware>& app);

} // namespace USER 