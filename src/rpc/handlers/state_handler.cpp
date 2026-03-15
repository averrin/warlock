#include <rpc/handlers/state_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <game/game_manager.hpp>

namespace rpc {

void registerStateHandlers(Server& server) {
  // state.save — save current state
  server.router().on("state.save", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw std::runtime_error("Game not started");
    }

    gm.enqueueCommand([&gm]() {
      gm.saveData();
    });

    return {{"status", "queued"}};
  });

  // state.load — load state from disk
  server.router().on("state.load", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();

    gm.enqueueCommand([&gm]() {
      gm.loadData();
    });

    return {{"status", "queued"}};
  });
}

} // namespace rpc
