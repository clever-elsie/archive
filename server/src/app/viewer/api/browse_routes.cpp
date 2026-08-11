#include <app/viewer/api/route_registration.hpp>

#include <algorithm>

#include <app/viewer/api/common.hpp>
#include <app/viewer/api/entry_json.hpp>
#include <app/viewer/api/query.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER::api {

void register_browse_routes(App& app) {
  CROW_ROUTE(app, "/req/viewer")
    .methods(crow::HTTPMethod::GET)
    ([](const crow::request& req) {
      auto& manager = Manager::get_instance();
      const auto view = manager.acquire_read();
      if (!view) return error_response(req, 503, "RELOAD_IN_PROGRESS", "viewer graph is not available", "root");
      const bool administrator = manager.is_admin_request(req);
      if (!manager.can_access(view.state(), view.state().root, administrator))
        return error_response(req, 403, "FORBIDDEN", "viewer root is not accessible");
      auto data = node_json(manager, view.state(), view.state().root, administrator);
      const auto* root = view.node(view.state().root);
      if (root) add_hidden_aliases(manager, view.state(), root->id, administrator, req, data);
      return crow::response(envelope(req, std::move(data)));
    });

  // 旧page_dataに相当する、通常のWorkとmixed Work内の独立Memberを
  // 更新時刻順に保持したキャッシュのページ取得。Collectionの子一覧とは
  // 異なり、randomと同じメディア単位を対象にする。
  CROW_ROUTE(app, "/req/viewer/page")
    .methods(crow::HTTPMethod::GET)
    ([](const crow::request& req) {
      auto& manager = Manager::get_instance();
      const auto view = manager.acquire_read();
      if (!view) return error_response(req, 503, "RELOAD_IN_PROGRESS", "viewer graph is not available", "root");
      const bool administrator = manager.is_admin_request(req);
      const auto* filter_value = req.url_params.get("filter");
      const std::string_view filter = filter_value ? std::string_view(filter_value) : std::string_view("all");
      std::vector<NodeRef> refs = view.state().media_page_cache[media_cache_index(filter)];
      std::erase_if(refs, [&](NodeRef ref) { return !manager.can_access(view.state(), ref, administrator); });
      const auto* sort_key = req.url_params.get("sort_key");
      const auto* direction = req.url_params.get("direction");
      const bool cached_order = (!sort_key || std::string_view(sort_key) == "updated_at") &&
                                (!direction || std::string_view(direction) == "desc");
      auto data = page_json(manager, view, std::move(refs), req, administrator, !cached_order);
      if (const auto* root = view.node(view.state().root))
        add_hidden_aliases(manager, view.state(), root->id, administrator, req, data);
      return crow::response(envelope(req, std::move(data)));
    });

  CROW_ROUTE(app, "/req/viewer/entries/<string>")
    .methods(crow::HTTPMethod::GET)
    ([](const crow::request& req, std::string id_text) {
      const auto id = parse_id(id_text);
      if (!id) return error_response(req, 400, "INVALID_ID", "entry id is invalid");
      auto& manager = Manager::get_instance();
      const auto view = manager.acquire_read();
      if (!view) return error_response(req, 503, "RELOAD_IN_PROGRESS", "viewer graph is not available", "root");
      return entry_response(req, manager, view, *id, manager.is_admin_request(req));
    });

  CROW_ROUTE(app, "/req/viewer/entries/<string>/children")
    .methods(crow::HTTPMethod::GET)
    ([](const crow::request& req, std::string id_text) {
      const auto id = parse_id(id_text);
      if (!id) return error_response(req, 400, "INVALID_ID", "entry id is invalid");
      auto& manager = Manager::get_instance();
      const auto view = manager.acquire_read();
      if (!view) return error_response(req, 503, "RELOAD_IN_PROGRESS", "viewer graph is not available", "root");
      const bool administrator = manager.is_admin_request(req);
      const auto found = view.state().index.find(*id);
      if (found == view.state().index.end()) return error_response(req, 404, "STALE_REFERENCE", "entry is not in the current graph", "root");
      const auto canonical = found->second.alias ? found->second.canonical_id : *id;
      const auto target = view.state().index.find(canonical);
      const bool accessible = found->second.alias
        ? ([&] {
            const auto* alias = find_alias(view.state(), *id);
            return alias && manager.can_access_alias(view.state(), *alias, administrator);
          }())
        : (target != view.state().index.end() && manager.can_access(view.state(), target->second.node, administrator));
      if (target == view.state().index.end() || target->second.node == invalid_node || !accessible)
        return error_response(req, 403, "FORBIDDEN", "entry is not accessible");
      const auto* node = view.node(target->second.node);
      if (!node) return error_response(req, 404, "STALE_REFERENCE", "entry is not available", "root");
      std::vector<NodeRef> children;
      const auto begin = node->first_child;
      const auto end = begin + node->child_count;
      for (std::uint32_t index = begin; index < end && index < view.state().arena.child_refs.size(); ++index) {
        const auto ref = view.state().arena.child_refs[index];
        if (manager.can_access(view.state(), ref, administrator)) children.push_back(ref);
      }
      const auto limit = query_flag(req, "all")
        ? std::optional<std::size_t>(std::max<std::size_t>(1, children.size()))
        : std::nullopt;
      auto data = page_json(manager, view, std::move(children), req, administrator, true, limit);
      add_hidden_aliases(manager, view.state(), target->second.canonical_id, administrator, req, data);
      return crow::response(envelope(req, std::move(data)));
    });
}

} // namespace VIEWER::api
