#pragma once
#include <crow.h>

#include "manager.hpp"
#include "api.hpp"
#include <manager/auth/middleware.hpp>
#include <manager/auth/authorization_middleware.hpp>


namespace USER {

using App = crow::App<MIDDLEWARE::AuthMiddleware, MIDDLEWARE::AuthorizationMiddleware>;
void setup(App& app);

} // namespace USER 