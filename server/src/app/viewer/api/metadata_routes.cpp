#include <app/viewer/api/route_registration.hpp>

#include <app/viewer/api/common.hpp>
#include <app/viewer/manager.hpp>

namespace VIEWER::api {

void register_metadata_routes(App& app) {
  CROW_ROUTE(app, "/req/viewer/entries/<string>/metadata")
    .methods(crow::HTTPMethod::PATCH)
    ([](const crow::request& req, std::string id_text) {
      auto& manager = Manager::get_instance();
      if (!manager.is_admin_request(req))
        return error_response(req, 403, "ADMIN_REQUIRED", "administrator permission is required");
      const auto view = manager.acquire_read();
      if (!view) return error_response(req, 503, "RELOAD_IN_PROGRESS", "viewer graph is not available", "root");
      const auto id = parse_id(id_text);
      if (!id) return error_response(req, 400, "INVALID_ID", "entry id is invalid");
      const auto json = crow::json::load(req.body);
      if (!json) return error_response(req, 400, "BAD_REQUEST", "metadata request is invalid");
      std::string operation;
      std::string tag;
      if (json.has("operation")) operation = json["operation"].s();
      else if (json.has("AD")) operation = json["AD"].s();
      if (json.has("tag")) tag = json["tag"].s();
      else if (json.has("data")) tag = json["data"].s();
      if (operation == "delete") operation = "remove";
      if ((operation != "add" && operation != "remove") || tag.empty())
        return error_response(req, 400, "BAD_REQUEST", "operation and tag are required");
      TagTransaction transaction;
      transaction.target_id = *id;
      transaction.add = operation == "add";
      transaction.tag = std::move(tag);
      auto future = manager.enqueue_tag(std::move(transaction));
      const auto result = future.get();
      if (!result.success) return error_response(req, 409, result.code, "tag update could not be applied", "parent");
      crow::json::wvalue data;
      data["canonical_id"] = std::to_string(result.canonical_id);
      data["tags"] = crow::json::wvalue::list(result.tags.begin(), result.tags.end());
      return crow::response(envelope(req, std::move(data)));
    });
}

} // namespace VIEWER::api
