#pragma once
#include <crow.h>

#include "memo.hpp"
#include <manager/auth/middleware.hpp>

namespace MEMO_ROUTES{

void setup_memo_routes(crow::App<MIDDLEWARE::AuthMiddleware>& app);

} // namespace MEMO_ROUTES