#include <app/viewer/api/route_registration.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>

#include <app/viewer/api/common.hpp>
#include <app/viewer/api/content.hpp>
#include <app/viewer/api/entry_json.hpp>
#include <app/viewer/manager.hpp>
#include <app/viewer/content/zip_reader.hpp>
#include <app/viewer/ordering.hpp>

namespace VIEWER::api {
namespace {

bool is_zip_path(const std::filesystem::path& path) {
  auto extension = path.extension().string();
  for (char& character : extension)
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  return extension == ".zip";
}

EntryId archive_member_id(EntryId archive_id, std::string_view name) {
  std::uint64_t hash = 1469598103934665603ull;
  const auto update = [&](unsigned char value) {
    hash ^= value;
    hash *= 1099511628211ull;
  };
  for (const auto character : std::string("archive:") + std::to_string(archive_id) + ":")
    update(static_cast<unsigned char>(character));
  for (const auto character : name)
    update(static_cast<unsigned char>(character));
  return hash == 0 ? 1 : hash;
}

} // namespace

void register_content_routes(App& app) {
  CROW_ROUTE(app, "/req/viewer/entries/<string>/archive")
    .methods(crow::HTTPMethod::GET)
    ([](const crow::request& req, std::string id_text) {
      const auto id = parse_id(id_text);
      if (!id) return error_response(req, 400, "INVALID_ID", "entry id is invalid");
      auto& manager = Manager::get_instance();
      const auto view = manager.acquire_read();
      if (!view) return error_response(req, 503, "RELOAD_IN_PROGRESS", "viewer graph is not available", "root");
      const bool administrator = manager.is_admin_request(req);
      const auto found = view.state().index.find(*id);
      if (found == view.state().index.end())
        return error_response(req, 404, "STALE_REFERENCE", "entry is not in the current graph", "root");

      NodeRef node_ref = found->second.node;
      if (found->second.alias) {
        const auto* alias = find_alias(view.state(), *id);
        const auto canonical = view.state().index.find(found->second.canonical_id);
        if (!alias || canonical == view.state().index.end() || canonical->second.node == invalid_node ||
            !manager.can_access_alias(view.state(), *alias, administrator))
          return error_response(req, 403, "FORBIDDEN", "entry is not accessible");
        node_ref = canonical->second.node;
      } else if (!manager.can_access(view.state(), node_ref, administrator)) {
        return error_response(req, 403, "FORBIDDEN", "entry is not accessible");
      }

      const auto* node = view.node(node_ref);
      if (!node || node->kind != NodeKind::member || node->media_type != MediaType::image)
        return error_response(req, 400, "INVALID_ARCHIVE", "entry is not an image member");
      const auto relative = view.state().text(node->source_path);
      if (!is_zip_path(std::filesystem::path(relative)))
        return error_response(req, 400, "INVALID_ARCHIVE", "entry is not an image archive");
      if (!manager.source_available(view.state(), node_ref)) {
        manager.mark_dirty();
        return error_response(req, 404, "SOURCE_UNAVAILABLE", "archive source is no longer available", "parent");
      }

      const auto entries = content::zip_reader::list_entries(manager.root_path() / std::filesystem::path(relative));
      crow::json::wvalue::list items;
      std::size_t order = 0;
      const auto parent = view.node(node->parent);
      for (const auto& entry : entries) {
        if (!content::zip_reader::is_image_entry(entry.name)) continue;
        const auto display = ordering::display_name(std::filesystem::path(entry.name).filename().generic_string());
        crow::json::wvalue item;
        item["id"] = std::to_string(archive_member_id(node->id, entry.name));
        item["kind"] = "member";
        item["media_type"] = "image";
        item["mime_type"] = content::zip_reader::mime_type(entry.name);
        item["display_name"] = display.base;
        if (!display.ruby.empty()) item["display_name_ruby"] = display.ruby;
        item["state"] = "available";
        item["parent_id"] = parent ? std::to_string(parent->id) : "0";
        item["order"] = order++;
        item["preview"] = order == 1;
        item["virtual"] = true;
        item["capabilities"]["read"] = true;
        item["content"]["href"] = "/req/viewer/content/" + std::to_string(node->id) +
                                     "?archive_member=" + encode_path(entry.name);
        item["content"]["supports_range"] = false;
        items.push_back(std::move(item));
      }
      crow::json::wvalue data;
      data["items"] = std::move(items);
      data["total"] = order;
      data["archive_id"] = std::to_string(node->id);
      return crow::response(envelope(req, std::move(data)));
    });

  CROW_ROUTE(app, "/req/viewer/content/<string>")
    .methods(crow::HTTPMethod::GET)
    ([](const crow::request& req, std::string id_text) {
      const auto id = parse_id(id_text);
      if (!id) return error_response(req, 400, "INVALID_ID", "member id is invalid");
      auto& manager = Manager::get_instance();
      const auto view = manager.acquire_read();
      if (!view) return error_response(req, 503, "RELOAD_IN_PROGRESS", "viewer graph is not available", "root");
      const bool administrator = manager.is_admin_request(req);
      const auto found = view.state().index.find(*id);
      if (found == view.state().index.end())
        return error_response(req, 404, "STALE_REFERENCE", "member is not in the current graph", "root");
      NodeRef node_ref = found->second.node;
      if (found->second.alias) {
        const auto* alias = find_alias(view.state(), *id);
        const auto canonical = view.state().index.find(found->second.canonical_id);
        if (!alias || canonical == view.state().index.end() || canonical->second.node == invalid_node ||
            !manager.can_access_alias(view.state(), *alias, administrator))
          return error_response(req, 403, "FORBIDDEN", "member is not accessible");
        node_ref = canonical->second.node;
      } else if (!manager.can_access(view.state(), node_ref, administrator)) {
        return error_response(req, 403, "FORBIDDEN", "member is not accessible");
      }
      const auto* node = view.node(node_ref);
      if (!node || node->kind != NodeKind::member)
        return error_response(req, 400, "INVALID_MEMBER", "entry is not a media member");
      const auto relative = view.state().text(node->source_path);
      if (!manager.source_available(view.state(), node_ref)) {
        manager.mark_dirty();
        return error_response(req, 404, "SOURCE_UNAVAILABLE", "member source is no longer available", "parent");
      }
      // ZIPの内部はグラフ構築時には走査しない。通常のpreview要求では
      // 名前順の先頭画像を返し、archive_member指定時にはその画像を返す。
      if (node->media_type == MediaType::image && is_zip_path(std::filesystem::path(relative))) {
        const auto archive = manager.root_path() / std::filesystem::path(relative);
        const auto entries = content::zip_reader::list_entries(archive);
        const char* requested_name = req.url_params.get("archive_member");
        const auto found_entry = std::ranges::find_if(entries, [&](const auto& entry) {
          return content::zip_reader::is_image_entry(entry.name) &&
                 (!requested_name || entry.name == requested_name);
        });
        if (found_entry == entries.end())
          return error_response(req, 404, "CONTENT_UNAVAILABLE", "archive has no readable image", "parent");
        const auto bytes = content::zip_reader::extract_entry(archive, *found_entry);
        if (!bytes)
          return error_response(req, 500, "ARCHIVE_READ_FAILED", "archive member could not be read");
        crow::response response;
        response.code = 200;
        response.body.assign(bytes->begin(), bytes->end());
        response.set_header("Content-Length", std::to_string(response.body.size()));
        response.set_header("Content-Type", content::zip_reader::mime_type(found_entry->name));
        response.set_header("X-Content-Type-Options", "nosniff");
        response.set_header("Cache-Control", "no-transform");
        return response;
      }
      crow::response response;
      response.code = 200;
      response.set_header("Accept-Ranges", "bytes");
      response.set_header("X-Content-Type-Options", "nosniff");
      response.set_header("X-Accel-Redirect", "/_protected_media/" + encode_path(relative));
      response.set_header("Content-Type", std::string(view.state().text(node->mime_type)));
      return response;
    });
}

} // namespace VIEWER::api
