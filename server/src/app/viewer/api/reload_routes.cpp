#include <app/viewer/api/route_registration.hpp>

#include <app/viewer/api/common.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER::api {

void register_reload_routes(App& app) {
  CROW_ROUTE(app, "/req/viewer/reload")
    .methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
      auto& manager = Manager::get_instance();
      if (!manager.is_admin_request(req))
        return error_response(req, 403, "ADMIN_REQUIRED", "administrator permission is required");
      const auto result = manager.request_reload(true);
      if (result.code == ReloadResult::Code::cooldown) {
        auto response = error_response(req, 429, "RELOAD_COOLDOWN", "reload minimum interval has not elapsed");
        response.set_header("Retry-After", std::to_string(result.retry_after.count() / 1000));
        return response;
      }
      if (result.code == ReloadResult::Code::already_pending)
        return error_response(req, 409, "RELOAD_ALREADY_PENDING", "reload is already pending or running");
      if (result.code == ReloadResult::Code::not_ready)
        return error_response(req, 503, "VIEWER_NOT_READY", "viewer reload worker is not available", "root");
      crow::json::wvalue data;
      data["accepted"] = true;
      return crow::response(202, envelope(req, std::move(data)));
    });
}

} // namespace VIEWER::api
