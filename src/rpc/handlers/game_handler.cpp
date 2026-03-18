#include <rpc/handlers/game_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <rpc/dto.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/well_known_entities.hpp>
#include <game/systems/environment.hpp>
#include <utils/entt.hpp>

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
      frames.push_back(serializeFrame(registry, entity, frame));
    }

    // Serialize all connections
    nlohmann::json connections = nlohmann::json::array();
    auto connView = registry.view<Connection>();
    for (auto entity : connView) {
      auto& conn = connView.get<Connection>(entity);
      connections.push_back(serializeConnection(entity, conn));
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

  // game.status — returns started/tick/seed
  server.router().on("game.status", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    auto& lua = entt::locator<sol::state>::value();
    uint64_t seed = lua["settings"]["seed"].get_or<uint64_t>(0);
    return {
      {"started", gm.started},
      {"tick", gm.tick_count()},
      {"seed", seed}
    };
  });

  // game.start — start the game
  server.router().on("game.start", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (gm.started) {
      return {{"status", "already_started"}};
    }
    bool do_new = false;
    if (params.contains("new") && params["new"].is_boolean()) {
      do_new = params["new"].get<bool>();
    }
    gm.enqueueCommand([&gm, do_new]() {
      if (do_new) {
        gm.loadData();
      }
      gm.start();
    });
    return {{"status", "starting"}};
  });

  // game.pause — pause the game loop
  server.router().on("game.pause", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    gm.setPaused(true);
    return nlohmann::json::object();
  });

  // game.resume — resume the game loop
  server.router().on("game.resume", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    gm.setPaused(false);
    return nlohmann::json::object();
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
      result["is_day"] = env.isDay;
    }
    return result;
  });

  // env.status — returns environment state
  server.router().on("env.status", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& wk = entt::locator<WellKnownEntities>::value();
    auto& reg = entt::locator<State>::value().registry;
    if (wk.environment == entt::null || !reg.valid(wk.environment) || !reg.all_of<Environment>(wk.environment)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "No environment entity"};
    }
    auto& env = reg.get<Environment>(wk.environment);
    return nlohmann::json{{"env", rpc::serializeEnvironment(env)}};
  });
}

} // namespace rpc
