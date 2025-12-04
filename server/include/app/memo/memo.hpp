#pragma once
#include <cctype>
#include <algorithm>

#include "shared_memo.hpp"
#include <manager/auth/auth.hpp>
#include <app/retrieve.hpp>

namespace MEMO{
using namespace std;
using i64=int64_t;
using u64=uint64_t;

crow::response memo_fetch_all(const crow::request &req);
crow::response memo_search(const crow::request &req);
crow::response memo_create_new(const crow::request &req);
crow::response memo_renew(const crow::request &req);
crow::response memo_now(const crow::request &req);
crow::response memo_rm(const crow::request &req);
crow::response memo_rename(const crow::request &req);
crow::response memo_update_tags(const crow::request &req);
crow::response memo_get_formats(const crow::request &req);
crow::response memo_check_title(const crow::request &req);
crow::response memo_create_with_title(const crow::request &req);

}//namespace MEMO