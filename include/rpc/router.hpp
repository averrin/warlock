#pragma once
#include <rpc/message.hpp>
#include <functional>
#include <map>
#include <string>

namespace rpc {

// Connection context passed to handlers
struct Context {
  std::string connectionId;
};

// Handler takes context + params JSON and returns result JSON. Throws on error.
using Handler = std::function<nlohmann::json(const Context& ctx, const nlohmann::json& params)>;

class Router {
public:
  void on(const std::string& method, Handler handler);

  // Returns serialized JSON response string
  std::string dispatch(const std::string& raw_message, const Context& ctx = {});

private:
  std::map<std::string, Handler> handlers_;
};

} // namespace rpc
