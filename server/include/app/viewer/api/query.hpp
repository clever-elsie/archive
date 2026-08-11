#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <crow.h>

#include <app/viewer/manager.hpp>

namespace VIEWER::api {

bool query_flag(const crow::request& req, const char* name);
std::size_t query_size(const crow::request& req, const char* name,
                       std::size_t fallback, std::size_t maximum);
std::size_t media_cache_index(std::string_view filter);
void sort_nodes(const GraphState& state, std::vector<NodeRef>& refs, const crow::request& req);
crow::json::wvalue page_json(const Manager& manager, const ReadView& view,
                             std::vector<NodeRef> refs, const crow::request& req,
                             bool administrator, bool sort = true,
                             std::optional<std::size_t> forced_limit = std::nullopt);
bool contains_query(const GraphState& state, const NodeRecord& node, std::string_view query);
bool matches_filter(const GraphState& state, NodeRef ref, std::string_view filter);

} // namespace VIEWER::api
