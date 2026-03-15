#include <rpc/handlers/session_handler.hpp>
#include <stdexcept>

namespace rpc {

void registerSessionHandlers(Server& server) {
  server.router().on("session.claim", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    if (server.isClaimed()) {
      throw std::runtime_error("Session already claimed");
    }

    if (!server.claim(ctx.connectionId)) {
      throw std::runtime_error("Failed to claim session");
    }

    return {{"status", "claimed"}, {"connectionId", ctx.connectionId}};
  });

  server.router().on("session.release", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    if (!server.isOwner(ctx.connectionId)) {
      throw std::runtime_error("Not the session owner");
    }

    server.release(ctx.connectionId);
    return {{"status", "released"}};
  });

  server.router().on("session.info", [&server](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    return {
      {"claimed", server.isClaimed()},
      {"owner", server.ownerConnectionId()},
      {"clientCount", server.clientCount()}
    };
  });
}

} // namespace rpc
