#pragma once

#include <crow.h>

#include <app/viewer/manager.hpp>

namespace VIEWER::api {

const AliasRecord* find_alias(const GraphState& state, EntryId id);
crow::json::wvalue alias_json(const GraphState& state, const AliasRecord& alias);
crow::json::wvalue node_json(const Manager& manager, const GraphState& state, NodeRef ref, bool administrator);
void add_hidden_aliases(const Manager& manager, const GraphState& state, EntryId parent_id,
                        bool administrator, const crow::request& req, crow::json::wvalue& data);
crow::response entry_response(const crow::request& req, const Manager& manager,
                              const ReadView& view, EntryId id, bool administrator);

} // namespace VIEWER::api
