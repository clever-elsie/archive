#include <app/viewer/routes.hpp>

#include <app/viewer/api/route_registration.hpp>
#include <app/viewer/manager.hpp>
#include <manager/config.hpp>

namespace VIEWER {

void setup(App& app, std::string&& viewer_dir) {
  auto& manager = Manager::get_instance();
  manager.configure(std::move(viewer_dir), CONFIG::params.VIEWER_PUB_LIST,
                    std::chrono::seconds(CONFIG::params.VIEWER_SCAN_INTERVAL_SECONDS));

  api::register_browse_routes(app);
  api::register_search_routes(app);
  api::register_random_routes(app);
  api::register_content_routes(app);
  api::register_metadata_routes(app);
  api::register_reload_routes(app);
  api::register_status_routes(app);
}

} // namespace VIEWER
