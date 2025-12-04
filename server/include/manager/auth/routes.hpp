#pragma once
#include <crow.h>

#include "auth.hpp"
#include "middleware.hpp"

namespace AUTH{

void setup(crow::App<MIDDLEWARE::AuthMiddleware>& app);

} // namespace AUTH