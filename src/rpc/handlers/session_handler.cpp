#include <rpc/handlers/session_handler.hpp>
#include <rpc/message.hpp>

namespace rpc {

void registerSessionHandlers(Server& server) {
  server.router().on("session.claim", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    if (server.isClaimed()) {
      throw rpc::RpcError{rpc::error::NOT_CLAIMED, "Session already claimed"};
    }

    if (!server.claim(ctx.connectionId)) {
      throw rpc::RpcError{rpc::error::NOT_CLAIMED, "Failed to claim session"};
    }

    return {{"ok", true}};
  });

  server.router().on("session.release", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    if (!server.isOwner(ctx.connectionId)) {
      throw rpc::RpcError{rpc::error::NOT_CLAIMED, "Session not claimed by this connection"};
    }

    server.release(ctx.connectionId);
    return {{"status", "released"}};
  });

  server.router().on("session.info", [&server](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    std::string owner = server.ownerConnectionId();
    nlohmann::json claimed_by = owner.empty() ? nlohmann::json(nullptr) : nlohmann::json(owner);
    return {
      {"claimed_by", claimed_by},
      {"clients", server.clientCount()},
      {"uptime_ms", server.uptimeMs()}
    };
  });
}

} // namespace rpc
