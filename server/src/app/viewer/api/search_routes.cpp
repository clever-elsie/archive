#include <app/viewer/api/route_registration.hpp>

#include <string_view>

#include <app/viewer/api/common.hpp>
#include <app/viewer/api/query.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER::api {

void register_search_routes(App& app) {
  CROW_ROUTE(app, "/req/viewer/search")
    .methods(crow::HTTPMethod::GET)
    ([](const crow::request& req) {
      auto& manager = Manager::get_instance();
      const auto view = manager.acquire_read();
      if (!view) return error_response(req, 503, "RELOAD_IN_PROGRESS", "viewer graph is not available", "root");
      const auto* query = req.url_params.get("query");
      if (!query || std::string_view(query).empty()) return error_response(req, 400, "INVALID_QUERY", "query is required");
      const auto parsed_query = parse_search_query(query);
      if (!parsed_query)
        return error_response(req, 400, "INVALID_QUERY", "query syntax is invalid");
      const bool administrator = manager.is_admin_request(req);
      std::vector<NodeRef> matches;
      for (NodeRef ref = 0; ref < view.state().arena.nodes.size(); ++ref) {
        const auto* node = view.node(ref);
        if (!node || node->kind != NodeKind::work ||
            (node->flags & node_attached_media_flag) != 0) continue;
        if (manager.can_access(view.state(), ref, administrator) &&
            matches_query(view.state(), *node, *parsed_query))
          matches.push_back(ref);
      }
      return crow::response(envelope(req, page_json(manager, view, std::move(matches), req, administrator)));
    });
}

} // namespace VIEWER::api
