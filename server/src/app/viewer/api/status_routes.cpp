#include <app/viewer/api/route_registration.hpp>

#include <utility>

#include <app/viewer/api/common.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER::api {

void register_status_routes(App& app) {
  CROW_ROUTE(app, "/req/viewer/status")
    .methods(crow::HTTPMethod::GET)
    ([](const crow::request& req) {
      const auto state = Manager::get_instance().viewer_state();
      crow::json::wvalue data;
      switch (state) {
        case ViewerState::ready: data["state"] = "ready"; break;
        case ViewerState::reloading: data["state"] = "reloading"; break;
        case ViewerState::unavailable: data["state"] = "unavailable"; break;
      }
      return crow::response(envelope(req, std::move(data)));
    });
}

} // namespace VIEWER::api
