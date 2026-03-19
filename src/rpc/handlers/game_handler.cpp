#include <rpc/handlers/game_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <rpc/dto.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/well_known_entities.hpp>
#include <game/systems/environment.hpp>
#include <game/nexus_api.hpp>
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
      nlohmann::json result = {{"status", "already_started"}};
      logWebAction(server, "game.start", "already_started");
      return result;
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
    nlohmann::json result = {{"status", "starting"}};
    logWebAction(server, "game.start", "starting", {{"new", do_new}});
    return result;
  });

  // game.pause — pause the game loop
  server.router().on("game.pause", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    gm.setPaused(true);
    logWebAction(server, "game.pause", "ok");
    return nlohmann::json::object();
  });

  // game.resume — resume the game loop
  server.router().on("game.resume", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    gm.setPaused(false);
    logWebAction(server, "game.resume", "ok");
    return nlohmann::json::object();
  });

  // game.speed.set — set game speed multiplier (1, 5, 10)
  server.router().on("game.speed.set", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    int multiplier = 1;
    if (params.contains("multiplier") && params["multiplier"].is_number_integer()) {
      multiplier = params["multiplier"].get<int>();
    }
    if (multiplier != 1 && multiplier != 5 && multiplier != 10) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "multiplier must be one of: 1, 5, 10"};
    }
    gm.setSpeedMultiplier(static_cast<float>(multiplier));
    auto effective = gm.speedMultiplier();
    nlohmann::json result = {{"multiplier", effective}};
    logWebAction(server, "game.speed.set", "ok", {{"multiplier", effective}});
    return result;
  });

  // game.speed.get — get current speed state
  server.router().on("game.speed.get", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    return {
      {"paused", gm.paused()},
      {"multiplier", gm.speedMultiplier()}
    };
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

  // env.status — returns environment state
  server.router().on("env.status", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& wk = entt::locator<WellKnownEntities>::value();
    auto& reg = entt::locator<State>::value().registry;
    if (wk.environment == entt::null || !reg.valid(wk.environment) || !reg.all_of<Environment>(wk.environment)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "No environment entity"};
    }
    auto& env = reg.get<Environment>(wk.environment);
    auto result = rpc::serializeEnvironment(env);

    // Export stable chart keys from the environment history buffers.
    auto toJsonArray = [](const std::deque<float>& values) {
      nlohmann::json out = nlohmann::json::array();
      for (float value : values) {
        out.push_back(value);
      }
      return out;
    };
    const auto findHistory = [&env](const std::string& key) -> const std::deque<float>* {
      auto it = env.named_history.find(key);
      if (it == env.named_history.end()) {
        return nullptr;
      }
      return &it->second;
    };

    nlohmann::json history = nlohmann::json::object();
    if (const auto* values = findHistory("temperature")) {
      history["temperature"] = toJsonArray(*values);
    } else {
      history["temperature"] = nlohmann::json::array();
    }
    if (const auto* values = findHistory("air_flow")) {
      history["air_flow"] = toJsonArray(*values);
    } else if (const auto* values = findHistory("airFlow")) {
      history["air_flow"] = toJsonArray(*values);
    } else {
      history["air_flow"] = nlohmann::json::array();
    }
    if (const auto* values = findHistory("sun")) {
      history["sun"] = toJsonArray(*values);
    } else {
      history["sun"] = nlohmann::json::array();
    }
    result["history"] = history;

    return nlohmann::json{{"env", result}};
  });

  // input.marker_set
  server.router().on("input.marker_set", [&server](const Context& /*ctx*/, const nlohmann::json& params) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (gm.started) {
      float x = params.value("x", 0.0f);
      float y = params.value("y", 0.0f);
      std::string label = params.value("label", "");
      std::string color = params.value("color", "");
      if (!label.empty()) {
        NexusApi nexus;
        nexus.setMapMarker(x, y, label, color);
      }
    }
    return {{"ok", true}};
  });

  // input.mouse_coords
  server.router().on("input.mouse_coords", [&server](const Context& /*ctx*/, const nlohmann::json& params) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (gm.started) {
      float x = params.value("x", 0.0f);
      float y = params.value("y", 0.0f);
      auto& state = entt::locator<State>::value();
      auto& wk = entt::locator<WellKnownEntities>::value();
      if (wk.environment != entt::null && state.registry.valid(wk.environment) && state.registry.all_of<Environment>(wk.environment)) {
        auto& env = state.registry.get<Environment>(wk.environment);
        env.mouseX = x;
        env.mouseY = y;
      }
    }
    return {{"ok", true}};
  });

  // input.marker_remove
  server.router().on("input.marker_remove", [&server](const Context& /*ctx*/, const nlohmann::json& params) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (gm.started) {
      std::string label = params.value("label", "");
      if (!label.empty()) {
        NexusApi nexus;
        nexus.removeMapMarker(label);
      }
    }
    return {{"ok", true}};
  });

  // env.set_field — mutate a single environment field
  server.router().on("env.set_field", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }

    if (!params.contains("field") || !params["field"].is_string()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "missing 'field' string"};
    }
    if (!params.contains("value") || !params["value"].is_number()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "missing 'value' number"};
    }

    std::string field = params["field"].get<std::string>();
    float value = params["value"].get<float>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& wk = entt::locator<WellKnownEntities>::value();
    auto& reg = entt::locator<State>::value().registry;
    if (wk.environment == entt::null || !reg.valid(wk.environment) || !reg.all_of<Environment>(wk.environment)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "No environment entity"};
    }
    auto& env = reg.get<Environment>(wk.environment);

    if (field == "temperature") {
      env.temperature = value;
    } else if (field == "air_flow") {
      env.airFlow = value;
    } else if (field == "sun") {
      env.sun = value;
    } else if (field == "minutes") {
      env.minutes = static_cast<int>(value);
    } else if (field == "days") {
      env.days = static_cast<int>(value);
    } else {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS,
        "unknown field '" + field + "'; valid: temperature, air_flow, sun, minutes, days"};
    }

    logWebAction(server, "env.set_field", "ok", {{"field", field}, {"value", value}});
    return {{"ok", true}};
  });
}

} // namespace rpc
