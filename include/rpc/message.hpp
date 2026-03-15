#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>

namespace rpc {

struct Request {
  std::string jsonrpc = "2.0";
  std::string method;
  nlohmann::json params = nlohmann::json::object();
  nlohmann::json id;  // string or int

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

// Standard JSON-RPC error codes
namespace error {
  constexpr int PARSE_ERROR = -32700;
  constexpr int INVALID_REQUEST = -32600;
  constexpr int METHOD_NOT_FOUND = -32601;
  constexpr int INVALID_PARAMS = -32602;
  constexpr int INTERNAL_ERROR = -32603;
  // Application-specific
  constexpr int NOT_CLAIMED = -32000;
  constexpr int ALREADY_CLAIMED = -32001;
  constexpr int READ_ONLY = -32002;
}

} // namespace rpc
