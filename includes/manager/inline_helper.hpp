#pragma once
#include "crow/http_response.h"
#include "crow/json.h"
#include <string>
using namespace std;

inline crow::response default_response(bool success, string&&message, int code=-1)noexcept{
  crow::json::wvalue response;
  response["success"] = success;
  response["message"] = std::move(message);
  if(code==-1) code = success ? 200 : 400;
  return crow::response(code, response);
}
