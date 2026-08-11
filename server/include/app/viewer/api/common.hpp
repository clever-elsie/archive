#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <crow.h>

#include <app/viewer/model/types.hpp>

namespace VIEWER::api {

std::optional<EntryId> parse_id(std::string_view value);
std::string request_id(const crow::request& req);
crow::json::wvalue envelope(const crow::request& req, crow::json::wvalue data);
crow::response error_response(const crow::request& req, int status, std::string_view code,
                              std::string_view message, std::string_view refresh = {});

} // namespace VIEWER::api
