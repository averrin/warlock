#include <rpc/handlers/game_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <rpc/dto.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/well_known_entities.hpp>

namespace rpc {

void registerGameHandlers(Server& server) {
  // game.state — full game state snapshot
  server.router().on("game.state", [&server](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      return {{"started", false}};
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    // Serialize all frames
    nlohmann::json frames = nlohmann::json::array();
    auto view = registry.view<Frame>();
    for (auto entity : view) {
      auto& frame = view.get<Frame>(entity);
      auto j = serializeFrame(frame);
      j["entityId"] = static_cast<int>(entity);
      frames.push_back(j);
    }

    // Serialize all connections
    nlohmann::json connections = nlohmann::json::array();
    auto connView = registry.view<Connection>();
    for (auto entity : connView) {
      auto& conn = connView.get<Connection>(entity);
      connections.push_back(serializeConnection(conn));
    }

    // Serialize environment
    nlohmann::json envJson = nullptr;
    auto& wk = entt::locator<WellKnownEntities>::value();
    if (wk.environment != entt::null && registry.valid(wk.environment) && registry.all_of<Environment>(wk.environment)) {
      auto& env = registry.get<Environment>(wk.environment);
      envJson = serializeEnvironment(env);
    }

    return {
      {"started", gm.started},
      {"frames", frames},
      {"connections", connections},
      {"environment", envJson}
    };
  });

  // game.start — start the game
  server.router().on("game.start", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (gm.started) {
      return {{"status", "already_started"}};
    }
    gm.enqueueCommand([&gm]() {
      gm.start();
    });
    return {{"status", "starting"}};
  });

  // game.tick — current timing info
  server.router().on("game.tick", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      return {{"started", false}};
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;
    auto& wk = entt::locator<WellKnownEntities>::value();

    nlohmann::json result = {{"started", true}};
    if (wk.environment != entt::null && registry.valid(wk.environment) && registry.all_of<Environment>(wk.environment)) {
      auto& env = registry.get<Environment>(wk.environment);
      result["minutes"] = env.minutes;
      result["days"] = env.days;
      result["isDay"] = env.isDay;
    }
    return result;
  });
}

} // namespace rpc
