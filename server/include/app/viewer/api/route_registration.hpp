#pragma once

#include <app/viewer/routes.hpp>

namespace VIEWER::api {

void register_browse_routes(App& app);
void register_search_routes(App& app);
void register_random_routes(App& app);
void register_content_routes(App& app);
void register_metadata_routes(App& app);
void register_reload_routes(App& app);
void register_status_routes(App& app);

} // namespace VIEWER::api
