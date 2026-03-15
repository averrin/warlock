#include <rpc/message.hpp>

namespace rpc {

std::optional<Request> Request::parse(const std::string& raw) {
  try {
    auto j = nlohmann::json::parse(raw);

    if (!j.contains("jsonrpc") || j["jsonrpc"] != "2.0") {
      return std::nullopt;
    }
    if (!j.contains("method") || !j["method"].is_string()) {
      return std::nullopt;
    }
    if (!j.contains("id")) {
      return std::nullopt;
    }

    Request req;
    req.jsonrpc = j["jsonrpc"].get<std::string>();
    req.method = j["method"].get<std::string>();
    req.id = j["id"];

    if (j.contains("params")) {
      req.params = j["params"];
    }

    return req;
  } catch (...) {
    return std::nullopt;
  }
}

std::string Response::serialize() const {
  nlohmann::json j;
  j["jsonrpc"] = jsonrpc;
  j["id"] = id;
  j["result"] = result;
  return j.dump();
}

std::string ErrorResponse::serialize() const {
  nlohmann::json j;
  j["jsonrpc"] = jsonrpc;
  j["id"] = id;
  j["error"] = {
    {"code", code},
    {"message", message}
  };
  if (!data.is_null()) {
    j["error"]["data"] = data;
  }
  return j.dump();
}

std::string Notification::serialize() const {
  nlohmann::json j;
  j["jsonrpc"] = jsonrpc;
  j["method"] = method;
  j["params"] = params;
  return j.dump();
}

} // namespace rpc
