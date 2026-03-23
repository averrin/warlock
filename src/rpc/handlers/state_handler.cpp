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

  // entities.list — enumerate all entities with their ECS component data
  server.router().on("entities.list", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      return {{"entities", nlohmann::json::array()}};
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    // Collect all alive entities
    nlohmann::json entities = nlohmann::json::array();
    for (auto entity : registry.storage<entt::entity>()->each()) {
      auto ent = std::get<0>(entity);
      if (!registry.valid(ent)) continue;

      nlohmann::json ent_json;
      ent_json["entity_id"] = static_cast<int>(ent);
      nlohmann::json comps = nlohmann::json::object();

      // meta
      if (registry.all_of<hf::meta>(ent)) {
        auto& c = registry.get<hf::meta>(ent);
        comps["meta"] = {{"name", c.name}, {"description", c.description}, {"id", c.id}};
      }
      // visible
      if (registry.all_of<hf::visible>(ent)) {
        auto& c = registry.get<hf::visible>(ent);
        comps["visible"] = {{"type", c.type}, {"sign", c.sign}, {"hidden", c.hidden}, {"seeThrough", c.seeThrough}, {"passThrough", c.passThrough}};
      }
      // ineditor
      if (registry.all_of<hf::ineditor>(ent)) {
        auto& c = registry.get<hf::ineditor>(ent);
        comps["ineditor"] = {{"icon", c.icon}};
      }
      // glow
      if (registry.all_of<hf::glow>(ent)) {
        auto& c = registry.get<hf::glow>(ent);
        comps["glow"] = {{"distance", c.distance}, {"type", static_cast<int>(c.type)}, {"bright", c.bright}, {"flick", c.flick}, {"passive", c.passive}, {"pulse", c.pulse}};
      }
      // renderable
      if (registry.all_of<hf::renderable>(ent)) {
        auto& c = registry.get<hf::renderable>(ent);
        comps["renderable"] = {{"spriteKey", c.spriteKey}, {"fgColor", c.fgColor}, {"hasBg", c.hasBg}, {"bgColor", c.bgColor}, {"hasBorder", c.hasBorder}, {"borderColor", c.borderColor}, {"hidden", c.hidden}, {"zIndex", c.zIndex}, {"fgLayer", c.fgLayer}, {"bgLayer", c.bgLayer}, {"brdLayer", c.brdLayer}};
      }
      // wall
      if (registry.all_of<hf::wall>(ent)) {
        comps["wall"] = nlohmann::json::object();
      }
      // tags
      if (registry.all_of<hf::tags>(ent)) {
        auto& c = registry.get<hf::tags>(ent);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& t : c.tags) arr.push_back(t);
        comps["tags"] = {{"tags", arr}};
      }
      // player
      if (registry.all_of<hf::player>(ent)) {
        comps["player"] = nlohmann::json::object();
      }
      // vision
      if (registry.all_of<hf::vision>(ent)) {
        auto& c = registry.get<hf::vision>(ent);
        comps["vision"] = {{"distance", c.distance}};
      }
      // obstacle
      if (registry.all_of<hf::obstacle>(ent)) {
        auto& c = registry.get<hf::obstacle>(ent);
        comps["obstacle"] = {{"passThrough", c.passThrough}, {"seeThrough", c.seeThrough}, {"interactive", c.interactive}, {"passAddCost", c.passAddCost}, {"interactionCost", c.interactionCost}};
      }
      // creature
      if (registry.all_of<hf::creature>(ent)) {
        comps["creature"] = nlohmann::json::object();
      }
      // script
      if (registry.all_of<hf::script>(ent)) {
        auto& c = registry.get<hf::script>(ent);
        comps["script"] = {{"path", c.path}, {"enabled", c.enabled}};
      }
      // Frame
      if (registry.all_of<Frame>(ent)) {
        auto& f = registry.get<Frame>(ent);
        comps["Frame"] = {{"id", f.data.id}, {"name", f.data.name}, {"size", std::string(magic_enum::enum_name(f.size))}, {"material", std::string(magic_enum::enum_name(f.material))}, {"component_count", static_cast<int>(f.components.size())}};
      }
      // Connection
      if (registry.all_of<Connection>(ent)) {
        auto& c = registry.get<Connection>(ent);
        comps["Connection"] = {{"id", c.data.id}, {"source", c.source}, {"target", c.target}, {"type", std::string(magic_enum::enum_name(c.type))}, {"medium", std::string(magic_enum::enum_name(c.medium))}};
      }
      // Environment
      if (registry.all_of<Environment>(ent)) {
        auto& c = registry.get<Environment>(ent);
        comps["Environment"] = {{"temperature", c.temperature}, {"air_flow", c.airFlow}, {"sun", c.sun}, {"minutes", c.minutes}, {"days", c.days}, {"radioactivity", c.radioactivity}};
      }
      // transform
      if (registry.all_of<wl::transform>(ent)) {
        auto& c = registry.get<wl::transform>(ent);
        comps["transform"] = {{"x", c.position.x}, {"y", c.position.y}, {"scale", c.scale}, {"rotation", c.rotation}, {"relative", c.relative}, {"layer", c.layer}};
      }
      // sprite
      if (registry.all_of<wl::sprite>(ent)) {
        auto& c = registry.get<wl::sprite>(ent);
        comps["sprite"] = {{"key", c.key}, {"width", c.rect.width}, {"height", c.rect.height}};
      }
      // relation
      if (registry.all_of<wl::relation>(ent)) {
        auto& c = registry.get<wl::relation>(ent);
        nlohmann::json children = nlohmann::json::array();
        for (auto ch : c.children) children.push_back(static_cast<int>(ch));
        comps["relation"] = {{"parent", c.parent == entt::null ? -1 : static_cast<int>(c.parent)}, {"children", children}};
      }
      // text
      if (registry.all_of<wl::text>(ent)) {
        auto& c = registry.get<wl::text>(ent);
        comps["text"] = {{"content", c.content}, {"size", c.size}};
      }
      // ResourcePatch
      if (registry.all_of<ResourcePatch>(ent)) {
        auto& c = registry.get<ResourcePatch>(ent);
        comps["ResourcePatch"] = {{"patch_type", c.patch_type}, {"item_name", c.item_name}, {"obstacle", c.obstacle}, {"cell_count", static_cast<int>(c.cells.size())}};
      }
      // tags: proto, item
      if (registry.all_of<entt::tag<"proto"_hs>>(ent)) {
        comps["proto"] = true;
      }
      if (registry.all_of<entt::tag<"item"_hs>>(ent)) {
        comps["item"] = true;
      }

      // Determine a label for the entity
      std::string label;
      if (registry.all_of<hf::meta>(ent)) {
        auto& m = registry.get<hf::meta>(ent);
        if (!m.name.empty()) label = m.name;
      }
      if (label.empty() && registry.all_of<Frame>(ent)) {
        label = "Frame: " + registry.get<Frame>(ent).data.name;
      }
      if (label.empty() && registry.all_of<Connection>(ent)) {
        label = "Connection #" + std::to_string(registry.get<Connection>(ent).data.id);
      }
      if (label.empty() && registry.all_of<Environment>(ent)) {
        label = "Environment";
      }
      if (label.empty() && registry.all_of<ResourcePatch>(ent)) {
        label = "Patch: " + registry.get<ResourcePatch>(ent).patch_type;
      }

      ent_json["label"] = label;
      ent_json["components"] = comps;
      entities.push_back(ent_json);
    }

    return {{"entities", entities}};
  });

  // entities.set_field — update a single field on an ECS component of an entity
  server.router().on("entities.set_field", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }

    int entity_id = params.at("entity_id").get<int>();
    std::string component = params.at("component").get<std::string>();
    std::string field = params.at("field").get<std::string>();
    const auto& value = params.at("value");

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto ent = static_cast<entt::entity>(entity_id);
    if (!registry.valid(ent)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Invalid entity"};
    }

    // macro-style dispatch for each component type
    if (component == "meta" && registry.all_of<hf::meta>(ent)) {
      auto& c = registry.get<hf::meta>(ent);
      if (field == "name") c.name = value.get<std::string>();
      else if (field == "description") c.description = value.get<std::string>();
      else if (field == "id") c.id = value.get<std::string>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "visible" && registry.all_of<hf::visible>(ent)) {
      auto& c = registry.get<hf::visible>(ent);
      if (field == "type") c.type = value.get<std::string>();
      else if (field == "sign") c.sign = value.get<std::string>();
      else if (field == "hidden") c.hidden = value.get<bool>();
      else if (field == "seeThrough") c.seeThrough = value.get<bool>();
      else if (field == "passThrough") c.passThrough = value.get<bool>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "ineditor" && registry.all_of<hf::ineditor>(ent)) {
      auto& c = registry.get<hf::ineditor>(ent);
      if (field == "icon") c.icon = value.get<std::string>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "glow" && registry.all_of<hf::glow>(ent)) {
      auto& c = registry.get<hf::glow>(ent);
      if (field == "distance") c.distance = value.get<float>();
      else if (field == "bright") c.bright = value.get<int>();
      else if (field == "flick") c.flick = value.get<int>();
      else if (field == "passive") c.passive = value.get<bool>();
      else if (field == "pulse") c.pulse = value.get<int>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "renderable" && registry.all_of<hf::renderable>(ent)) {
      auto& c = registry.get<hf::renderable>(ent);
      if (field == "spriteKey") c.spriteKey = value.get<std::string>();
      else if (field == "fgColor") c.fgColor = value.get<std::string>();
      else if (field == "hasBg") c.hasBg = value.get<bool>();
      else if (field == "bgColor") c.bgColor = value.get<std::string>();
      else if (field == "hasBorder") c.hasBorder = value.get<bool>();
      else if (field == "borderColor") c.borderColor = value.get<std::string>();
      else if (field == "hidden") c.hidden = value.get<bool>();
      else if (field == "zIndex") c.zIndex = value.get<int>();
      else if (field == "fgLayer") c.fgLayer = value.get<std::string>();
      else if (field == "bgLayer") c.bgLayer = value.get<std::string>();
      else if (field == "brdLayer") c.brdLayer = value.get<std::string>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "tags" && registry.all_of<hf::tags>(ent)) {
      auto& c = registry.get<hf::tags>(ent);
      if (field == "tags") {
        c.tags.clear();
        for (const auto& t : value) c.tags.push_back(t.get<std::string>());
      } else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "vision" && registry.all_of<hf::vision>(ent)) {
      auto& c = registry.get<hf::vision>(ent);
      if (field == "distance") c.distance = value.get<float>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "obstacle" && registry.all_of<hf::obstacle>(ent)) {
      auto& c = registry.get<hf::obstacle>(ent);
      if (field == "passThrough") c.passThrough = value.get<bool>();
      else if (field == "seeThrough") c.seeThrough = value.get<bool>();
      else if (field == "interactive") c.interactive = value.get<bool>();
      else if (field == "passAddCost") c.passAddCost = value.get<int>();
      else if (field == "interactionCost") c.interactionCost = value.get<int>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "script" && registry.all_of<hf::script>(ent)) {
      auto& c = registry.get<hf::script>(ent);
      if (field == "path") c.path = value.get<std::string>();
      else if (field == "enabled") c.enabled = value.get<bool>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "Environment" && registry.all_of<Environment>(ent)) {
      auto& c = registry.get<Environment>(ent);
      if (field == "temperature") c.temperature = value.get<float>();
      else if (field == "air_flow") c.airFlow = value.get<float>();
      else if (field == "sun") c.sun = value.get<float>();
      else if (field == "minutes") c.minutes = value.get<int>();
      else if (field == "days") c.days = value.get<int>();
      else if (field == "radioactivity") c.radioactivity = value.get<float>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "transform" && registry.all_of<wl::transform>(ent)) {
      auto& c = registry.get<wl::transform>(ent);
      if (field == "x") c.position.x = value.get<float>();
      else if (field == "y") c.position.y = value.get<float>();
      else if (field == "scale") c.scale = value.get<float>();
      else if (field == "rotation") c.rotation = value.get<float>();
      else if (field == "relative") c.relative = value.get<bool>();
      else if (field == "layer") c.layer = value.get<std::string>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "sprite" && registry.all_of<wl::sprite>(ent)) {
      auto& c = registry.get<wl::sprite>(ent);
      if (field == "key") c.key = value.get<std::string>();
      else if (field == "width") c.rect.width = value.get<float>();
      else if (field == "height") c.rect.height = value.get<float>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "text" && registry.all_of<wl::text>(ent)) {
      auto& c = registry.get<wl::text>(ent);
      if (field == "content") c.content = value.get<std::string>();
      else if (field == "size") c.size = value.get<int>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "ResourcePatch" && registry.all_of<ResourcePatch>(ent)) {
      auto& c = registry.get<ResourcePatch>(ent);
      if (field == "patch_type") c.patch_type = value.get<std::string>();
      else if (field == "item_name") c.item_name = value.get<std::string>();
      else if (field == "obstacle") c.obstacle = value.get<bool>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown component or entity does not have it"};
    }

    return {{"ok", true}};
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
