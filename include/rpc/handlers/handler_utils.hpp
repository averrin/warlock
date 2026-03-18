#pragma once
#include <rpc/message.hpp>
#include <rpc/server.hpp>
#include <stdexcept>

namespace rpc {

inline void requireClaim(const Server& server, const Context& ctx) {
  if (!server.isClaimed() || !server.isOwner(ctx.connectionId)) {
    throw rpc::RpcError{rpc::error::NOT_CLAIMED, "Session not claimed by this connection"};
  }
}

inline void logWebAction(Server& server,
                         const std::string& source,
                         const std::string& status,
                         const nlohmann::json& args = nlohmann::json::object()) {
  nlohmann::json params = {
      {"source", source},
      {"status", status},
  };
  if (!args.is_null() && !args.empty()) {
    params["args"] = args;
  }
  server.broadcast("web.log", params);
}

} // namespace rpc
