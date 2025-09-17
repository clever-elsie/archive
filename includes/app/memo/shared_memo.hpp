#pragma once
#include "data_structure.hpp"
#include "helper.hpp"

namespace MEMO{ // 共用メモ関連のAPIエンドポイント
using namespace std;
crow::json::wvalue format_for_response(const SharedMemoData& memo);
crow::response shared_memo_fetch_all(const crow::request &req);
crow::response shared_memo_create(const crow::request &req);
crow::response shared_memo_update(const crow::request &req);
crow::response shared_memo_delete(const crow::request &req);
crow::response shared_memo_get(const crow::request &req);
} // namespace MEMO