#pragma once
#include <crow.h>

#include "memo.hpp"
#include <manager/auth/middleware.hpp>

namespace MEMO{

void setup(crow::App<MIDDLEWARE::AuthMiddleware>& app);

} // namespace MEMO