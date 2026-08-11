#include <app/viewer/api/common.hpp>

#include <charconv>

namespace VIEWER::api {

std::optional<EntryId> parse_id(std::string_view value) {
  if (value.empty()) return std::nullopt;
  EntryId id = 0;
  const auto* first = value.data();
  const auto* last = value.data() + value.size();
  const auto result = std::from_chars(first, last, id);
  if (result.ec != std::errc{} || result.ptr != last || id == 0) return std::nullopt;
  return id;
}

std::string request_id(const crow::request& req) {
  const auto header = req.get_header_value("X-Request-Id");
  if (!header.empty()) return header;
  if (const auto* query = req.url_params.get("request_id")) return query;
  return {};
}

crow::json::wvalue envelope(const crow::request& req, crow::json::wvalue data) {
  crow::json::wvalue result;
  result["api_version"] = 1;
  result["success"] = true;
  result["code"] = "OK";
  const auto id = request_id(req);
  if (!id.empty()) result["request_id"] = id;
  result["data"] = std::move(data);
  result["diagnostics"] = nullptr;
  return result;
}

crow::response error_response(const crow::request& req, int status, std::string_view code,
                             std::string_view message, std::string_view refresh) {
  crow::json::wvalue body;
  body["api_version"] = 1;
  body["success"] = false;
  body["code"] = std::string(code);
  body["message"] = std::string(message);
  const auto id = request_id(req);
  if (!id.empty()) body["request_id"] = id;
  body["data"] = nullptr;
  body["error"]["code"] = std::string(code);
  body["error"]["message"] = std::string(message);
  body["error"]["retryable"] = (status == 409 || status == 429 || status == 503);
  if (!refresh.empty()) body["error"]["refresh"] = std::string(refresh);
  return crow::response(status, body);
}

} // namespace VIEWER::api
