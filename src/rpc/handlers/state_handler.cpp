#include <rpc/handlers/state_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <rpc/dto.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/thermal_aoe.hpp>
#include <game/well_known_entities.hpp>
#include <game/systems/power.hpp>
#include <game/components/resource_patch.hpp>
#include <game/patch_loader.hpp>

#include <mutex>
#include <vector>

namespace rpc {

namespace {

nlohmann::json serializeEnvironmentWithHistory(const Environment& env) {
  auto result = rpc::serializeEnvironment(env);

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

  return result;
}

nlohmann::json serializePowerNetworks() {
  nlohmann::json networks = nlohmann::json::array();
  if (!entt::locator<PowerInfo>::has_value()) {
    return networks;
  }

  auto& info = entt::locator<PowerInfo>::value();
  for (auto& net : info.networks) {
    nlohmann::json frames_arr = nlohmann::json::array();
    for (auto f : net.frames) {
      frames_arr.push_back(f);
    }

    nlohmann::json history_obj = nlohmann::json::object();
    for (auto& [k, dq] : net.history) {
      nlohmann::json h = nlohmann::json::array();
      for (auto v : dq) {
        h.push_back(v);
      }
      history_obj[k] = h;
    }

    networks.push_back({
      {"name", net.data.name},
      {"frames", frames_arr},
      {"production", net.production},
      {"consumption", net.consumption},
      {"accumulated", net.accumulated},
      {"accumulated_available", net.accumulated_available},
      {"battery_count", net.battery_count},
      {"history", history_obj}
    });
  }

  return networks;
}

} // namespace

void registerStateHandlers(Server& server) {
  // state.save — save current state
  server.router().on("state.save", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }

    gm.enqueueCommand([&gm, &server]() {
      gm.saveData();
      server.broadcast("notify.state.saved", {
        {"saved_at", gm.lastSavedAt()},
        {"auto", false}
      });
    });

    nlohmann::json result = {{"status", "queued"}};
    logWebAction(server, "state.save", "queued");
    return result;
  });

  // state.load — load state from disk
  server.router().on("state.load", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();

    gm.enqueueCommand([&gm]() {
      gm.loadData();
    });

    nlohmann::json result = {{"status", "queued"}};
    logWebAction(server, "state.load", "queued");
    return result;
  });

  // state.subscribe — register connection for topics
  server.router().on("state.subscribe", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    std::vector<std::string> topics;
    if (params.contains("topics") && params["topics"].is_array()) {
      for (const auto& t : params["topics"]) {
        if (t.is_string()) topics.push_back(t.get<std::string>());
      }
    }
    server.subscribe(ctx.connectionId, topics);
    return {{"ok", true}};
  });

  // state.unsubscribe — remove topics from connection's subscriptions
  server.router().on("state.unsubscribe", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    std::vector<std::string> topics;
    if (params.contains("topics") && params["topics"].is_array()) {
      for (const auto& t : params["topics"]) {
        if (t.is_string()) topics.push_back(t.get<std::string>());
      }
    }
    server.unsubscribe(ctx.connectionId, topics);
    return {{"ok", true}};
  });

  // state.snapshot — returns full state needed by web stores + scene payload
  server.router().on("state.snapshot", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      return {
        {"started", false},
        {"tick", gm.tick_count()},
        {"frames", nlohmann::json::array()},
        {"connections", nlohmann::json::array()},
        {"environment", nullptr},
        {"power_networks", nlohmann::json::array()},
        {"patches", nlohmann::json::array()},
        {"texts", nlohmann::json::array()},
        {"lines", nlohmann::json::array()},
        {"sprites", nlohmann::json::array()},
        {"hitboxes", nlohmann::json::array()}
      };
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    nlohmann::json frames = nlohmann::json::array();
    auto frameView = registry.view<Frame>();
    for (auto entity : frameView) {
      auto& frame = frameView.get<Frame>(entity);
      frames.push_back(serializeFrame(registry, entity, frame));
    }

    nlohmann::json connections = nlohmann::json::array();
    auto connView = registry.view<Connection>();
    for (auto entity : connView) {
      auto& conn = connView.get<Connection>(entity);
      connections.push_back(serializeConnection(entity, conn, gm.items.get()));
    }

    nlohmann::json envJson = nullptr;
    auto& wk = entt::locator<WellKnownEntities>::value();
    if (wk.environment != entt::null && registry.valid(wk.environment) && registry.all_of<Environment>(wk.environment)) {
      envJson = serializeEnvironmentWithHistory(registry.get<Environment>(wk.environment));
    }

    nlohmann::json patches = nlohmann::json::array();
    auto patchView = registry.view<ResourcePatch>();
    for (auto entity : patchView) {
      auto& patch = patchView.get<ResourcePatch>(entity);
      auto& meta = registry.get<hf::meta>(entity);
      nlohmann::json cells_arr = nlohmann::json::array();
      for (const auto& [x, y] : patch.cells) {
        cells_arr.push_back({x, y});
      }
      auto& loader = entt::locator<PatchLoader>::value();
      auto* def = loader.get_patch_type(patch.patch_type);
      nlohmann::json color = {{"r", 255}, {"g", 200}, {"b", 50}, {"a", 100}};
      if (def) {
        color = {{"r", def->color.r}, {"g", def->color.g}, {"b", def->color.b}, {"a", def->color.a}};
      }
      patches.push_back({
        {"id", static_cast<int>(entity)},
        {"name", meta.name},
        {"type", patch.patch_type},
        {"item", patch.item_name},
        {"obstacle", patch.obstacle},
        {"cells", cells_arr},
        {"bounds", {{"x", patch.min_x}, {"y", patch.min_y}, 
                    {"w", patch.max_x - patch.min_x + 1}, 
                    {"h", patch.max_y - patch.min_y + 1}}},
        {"color", color}
      });
    }

    nlohmann::json spendable = nlohmann::json::object();
    for (const auto& [k, v] : gm.spendablePool()) {
      spendable[k] = v;
    }

    return {
      {"started", gm.started},
      {"tick", gm.tick_count()},
      {"frames", frames},
      {"connections", connections},
      {"environment", envJson},
      {"spendable_pool", spendable},
      {"power_networks", serializePowerNetworks()},
      {"patches", patches},
      {"texts", nlohmann::json::array()},
      {"lines", nlohmann::json::array()},
      {"sprites", nlohmann::json::array()},
      {"hitboxes", nlohmann::json::array()}
    };
  });

  // state.autosave.configure — enable/disable autosave and set interval
  server.router().on("state.autosave.configure", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();

    if (params.contains("enabled") && params["enabled"].is_boolean()) {
      gm.autosave_enabled_ = params["enabled"].get<bool>();
    }
    if (params.contains("interval_seconds") && params["interval_seconds"].is_number()) {
      int secs = params["interval_seconds"].get<int>();
      if (secs < 5) secs = 5;
      if (secs > 3600) secs = 3600;
      gm.autosave_interval_ = std::chrono::seconds(secs);
    }

    logWebAction(server, "state.autosave.configure", "ok", {
      {"enabled", gm.autosave_enabled_},
      {"interval_seconds", gm.autosave_interval_.count()}
    });
    return {
      {"enabled", gm.autosave_enabled_},
      {"interval_seconds", gm.autosave_interval_.count()}
    };
  });

  // state.autosave.status — query current autosave config
  server.router().on("state.autosave.status", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    nlohmann::json result = {
      {"enabled", gm.autosave_enabled_},
      {"interval_seconds", gm.autosave_interval_.count()},
    };
    if (gm.lastSavedAt().empty()) {
      result["last_saved_at"] = nullptr;
    } else {
      result["last_saved_at"] = gm.lastSavedAt();
    }
    return result;
  });

  // state.thermalField — rectangular scalar field for map overlay (env.temperature + AoE sum)
  server.router().on("state.thermalField", [](const Context& /*ctx*/, const nlohmann::json& params) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INVALID_REQUEST, "Game not started"};
    }
    const int minGx = params.value("minGx", 0);
    const int minGy = params.value("minGy", 0);
    int width = params.value("width", 0);
    int height = params.value("height", 0);
    if (width <= 0 || height <= 0) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "width and height must be positive"};
    }
    constexpr int kMaxDim = 256;
    if (width > kMaxDim || height > kMaxDim) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "width and height must be <= 256"};
    }
    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;
    auto& wk = entt::locator<WellKnownEntities>::value();
    Environment* env = nullptr;
    if (wk.environment != entt::null && registry.valid(wk.environment) &&
        registry.all_of<Environment>(wk.environment)) {
      env = &registry.get<Environment>(wk.environment);
    }
    std::vector<float> values;
    warlock::thermal_aoe::fillThermalFieldRect(registry, env, minGx, minGy, width, height, values);
    nlohmann::json arr = nlohmann::json::array();
    for (float v : values) {
      arr.push_back(v);
    }
    return {
        {"minGx", minGx},
        {"minGy", minGy},
        {"width", width},
        {"height", height},
        {"values", arr},
    };
  });
}

} // namespace rpc
