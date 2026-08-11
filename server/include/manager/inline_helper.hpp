#pragma once
#include <string>
#include <utility>

#include <crow/http_response.h>
#include <crow/json.h>

using namespace std;

inline crow::response default_response(bool success, string&&message, int status_code=-1)noexcept{
  crow::json::wvalue response;
  const string message_copy = message;
  response["success"] = success;
  response["code"] = success ? "OK" : "REQUEST_FAILED";
  response["message"] = std::move(message);
  response["data"] = nullptr;
  if (!success)
    response["error"] = message_copy;
  if(status_code==-1) status_code = success ? 200 : 400;
  return crow::response(status_code, response);
}
