#include <app/viewer/api/route_registration.hpp>

#include <algorithm>
#include <random>
#include <string_view>

#include <app/viewer/api/common.hpp>
#include <app/viewer/api/entry_json.hpp>
#include <app/viewer/api/query.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER::api {

void register_random_routes(App& app) {
  CROW_ROUTE(app, "/req/viewer/random")
    .methods(crow::HTTPMethod::GET)
    ([](const crow::request& req) {
      auto& manager = Manager::get_instance();
      if (!manager.is_admin_request(req))
        return error_response(req, 403, "ADMIN_REQUIRED", "administrator permission is required");
      const auto view = manager.acquire_read();
      if (!view) return error_response(req, 503, "RELOAD_IN_PROGRESS", "viewer graph is not available", "root");
      constexpr bool administrator = true;
      const auto filter = req.url_params.get("filter");
      const auto filter_text = filter ? std::string_view(filter) : std::string_view("all");
      std::vector<NodeRef> works = view.state().media_page_cache[media_cache_index(filter_text)];
      std::erase_if(works, [&](NodeRef ref) {
        return !matches_filter(view.state(), ref, filter_text) ||
               !manager.can_access(view.state(), ref, administrator);
      });

      std::mt19937_64 random(std::random_device{}());
      std::shuffle(works.begin(), works.end(), random);
      constexpr std::size_t max_random_count = 500;
      const auto count = query_size(req, "count", max_random_count, max_random_count);
      if (works.size() > count) works.resize(count);

      crow::json::wvalue::list items;
      items.reserve(works.size());
      for (const auto ref : works) items.emplace_back(node_json(manager, view.state(), ref, administrator));
      crow::json::wvalue data;
      data["items"] = std::move(items);
      data["total"] = works.size();
      return crow::response(envelope(req, std::move(data)));
    });
}

} // namespace VIEWER::api
