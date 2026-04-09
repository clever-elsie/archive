#pragma once
#include <crow.h>

#include "auth.hpp"
#include "middleware.hpp"
#include <manager/auth/authorization_middleware.hpp>

namespace AUTH{

using App = crow::App<MIDDLEWARE::AuthMiddleware, MIDDLEWARE::AuthorizationMiddleware>;
void setup(App& app);

} // namespace AUTH