#pragma once
#include <crow.h>

#include "memo.hpp"
#include <manager/auth/middleware.hpp>
#include <manager/auth/authorization_middleware.hpp>

namespace MEMO{

using App = crow::App<MIDDLEWARE::AuthMiddleware, MIDDLEWARE::AuthorizationMiddleware>;
void setup(App& app);

} // namespace MEMO