#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace rpc {

struct Request {
  std::string jsonrpc = "2.0";
  std::string method;
  nlohmann::json params = nlohmann::json::object();
  std::optional<nlohmann::json> id;  // string or int; absent for notifications

  static std::optional<Request> parse(const std::string& raw);
};

struct Response {
  std::string jsonrpc = "2.0";
  nlohmann::json id;
  nlohmann::json result;

  std::string serialize() const;
};

struct ErrorResponse {
  std::string jsonrpc = "2.0";
  nlohmann::json id;
  int code;
  std::string message;
  nlohmann::json data = nullptr;

  std::string serialize() const;
};

struct Notification {
  std::string jsonrpc = "2.0";
  std::string method;
  nlohmann::json params = nlohmann::json::object();

  std::string serialize() const;
};

// Application error — caught by Router::dispatch and turned into an error response
struct RpcError : public std::exception {
  int code;
  std::string message;
  RpcError(int code, std::string msg) : code(code), message(std::move(msg)) {}
  const char* what() const noexcept override { return message.c_str(); }
};

// Standard JSON-RPC error codes
namespace error {
  constexpr int PARSE_ERROR      = -32700;
  constexpr int INVALID_REQUEST  = -32600;
  constexpr int METHOD_NOT_FOUND = -32601;
  constexpr int INVALID_PARAMS   = -32602;
  constexpr int INTERNAL_ERROR   = -32603;
  // Application-specific (positive range per spec)
  constexpr int NOT_CLAIMED       = 1000;
  constexpr int ENTITY_NOT_FOUND  = 1002;
  constexpr int INVALID_COMPONENT = 1003;
}

} // namespace rpc
