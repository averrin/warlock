#include <rpc/handlers/state_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <rpc/dto.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/thermal_aoe.hpp>
#include <game/well_known_entities.hpp>
#include <game/systems/power.hpp>
#include <game/components/frame.hpp>
#include <game/components/resource_patch.hpp>
#include <game/patch_loader.hpp>

#include <magic_enum.hpp>
#include <fmt/format.h>

#include <algorithm>
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

void removeEntityFromParentChildren(entt::registry& reg, entt::entity ent) {
  if (!reg.all_of<wl::relation>(ent)) return;
  auto& rel = reg.get<wl::relation>(ent);
  if (rel.parent == entt::null || !reg.valid(rel.parent) || !reg.all_of<wl::relation>(rel.parent)) return;
  auto& pch = reg.get<wl::relation>(rel.parent).children;
  pch.erase(std::remove(pch.begin(), pch.end(), ent), pch.end());
}

bool isProtectedWellKnown(entt::entity ent, const WellKnownEntities& wk) {
  return ent == wk.environment || ent == wk.economy || ent == wk.frames_folder ||
         ent == wk.connections_folder || ent == wk.patches_folder;
}

void emplaceComponentByName(entt::registry& reg, entt::entity ent, const std::string& name) {
  if (name == "meta") {
    if (!reg.all_of<hf::meta>(ent)) reg.emplace<hf::meta>(ent);
    return;
  }
  if (name == "ineditor") {
    if (!reg.all_of<hf::ineditor>(ent)) reg.emplace<hf::ineditor>(ent);
    return;
  }
  if (name == "tags") {
    if (!reg.all_of<hf::tags>(ent)) reg.emplace<hf::tags>(ent);
    return;
  }
  if (name == "player") {
    if (!reg.all_of<hf::player>(ent)) reg.emplace<hf::player>(ent);
    return;
  }
  if (name == "obstacle") {
    if (!reg.all_of<hf::obstacle>(ent)) reg.emplace<hf::obstacle>(ent);
    return;
  }
  if (name == "creature") {
    if (!reg.all_of<hf::creature>(ent)) reg.emplace<hf::creature>(ent);
    return;
  }
  if (name == "script") {
    if (!reg.all_of<hf::script>(ent)) reg.emplace<hf::script>(ent);
    return;
  }
  if (name == "Frame") {
    throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Use frame APIs for Frame"};
  }
  if (name == "Connection") {
    throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Use connection APIs for Connection"};
  }
  if (name == "Environment") {
    if (!reg.all_of<Environment>(ent)) reg.emplace<Environment>(ent);
    return;
  }
  if (name == "SpendablePool") {
    if (!reg.all_of<SpendablePool>(ent)) reg.emplace<SpendablePool>(ent);
    return;
  }
  if (name == "transform") {
    if (!reg.all_of<wl::transform>(ent)) reg.emplace<wl::transform>(ent);
    return;
  }
  if (name == "relation") {
    if (!reg.all_of<wl::relation>(ent)) reg.emplace<wl::relation>(ent);
    return;
  }
  if (name == "ResourcePatch") {
    throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Use patch APIs for ResourcePatch"};
  }
  if (name == "proto") {
    if (!reg.all_of<entt::tag<"proto"_hs>>(ent)) reg.emplace<entt::tag<"proto"_hs>>(ent);
    return;
  }
  throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown component: " + name};
}

void removeComponentByName(entt::registry& reg, entt::entity ent, const std::string& name) {
  if (name == "Frame" || name == "Connection" || name == "ResourcePatch") {
    throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Use dedicated APIs to remove " + name};
  }
  if (name == "Environment") {
    throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Cannot remove Environment component"};
  }
  if (name == "SpendablePool") {
    throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Cannot remove SpendablePool component"};
  }
  if (name == "relation") {
    if (!reg.all_of<wl::relation>(ent)) return;
    auto rel_copy = reg.get<wl::relation>(ent);
    removeEntityFromParentChildren(reg, ent);
    for (auto c : rel_copy.children) {
      if (reg.valid(c) && reg.all_of<wl::relation>(c)) {
        auto& cr = reg.get<wl::relation>(c);
        if (cr.parent == ent) cr.parent = entt::null;
      }
    }
    reg.remove<wl::relation>(ent);
    return;
  }
  if (name == "meta" && reg.all_of<hf::meta>(ent)) {
    reg.remove<hf::meta>(ent);
    return;
  }
  if (name == "ineditor" && reg.all_of<hf::ineditor>(ent)) {
    reg.remove<hf::ineditor>(ent);
    return;
  }
  if (name == "tags" && reg.all_of<hf::tags>(ent)) {
    reg.remove<hf::tags>(ent);
    return;
  }
  if (name == "player" && reg.all_of<hf::player>(ent)) {
    reg.remove<hf::player>(ent);
    return;
  }
  if (name == "obstacle" && reg.all_of<hf::obstacle>(ent)) {
    reg.remove<hf::obstacle>(ent);
    return;
  }
  if (name == "creature" && reg.all_of<hf::creature>(ent)) {
    reg.remove<hf::creature>(ent);
    return;
  }
  if (name == "script" && reg.all_of<hf::script>(ent)) {
    reg.remove<hf::script>(ent);
    return;
  }
  if (name == "transform" && reg.all_of<wl::transform>(ent)) {
    reg.remove<wl::transform>(ent);
    return;
  }
  if (name == "proto" && reg.all_of<entt::tag<"proto"_hs>>(ent)) {
    reg.remove<entt::tag<"proto"_hs>>(ent);
    return;
  }
  throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Component not present or unknown: " + name};
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
    for (auto tup : registry.storage<entt::entity>().each()) {
      auto ent = std::get<0>(tup);
      if (!registry.valid(ent)) continue;

      nlohmann::json ent_json;
      ent_json["entity_id"] = static_cast<int>(ent);
      nlohmann::json comps = nlohmann::json::object();

      // meta
      if (registry.all_of<hf::meta>(ent)) {
        auto& c = registry.get<hf::meta>(ent);
        comps["meta"] = {{"name", c.name}, {"description", c.description}, {"id", c.id}};
      }
      // ineditor
      if (registry.all_of<hf::ineditor>(ent)) {
        auto& c = registry.get<hf::ineditor>(ent);
        comps["ineditor"] = {{"icon", c.icon}, {"color", c.color}};
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
      // relation
      if (registry.all_of<wl::relation>(ent)) {
        auto& c = registry.get<wl::relation>(ent);
        nlohmann::json children = nlohmann::json::array();
        for (auto ch : c.children) children.push_back(static_cast<int>(ch));
        comps["relation"] = {{"parent", c.parent == entt::null ? -1 : static_cast<int>(c.parent)}, {"children", children}};
      }
      // ResourcePatch
      if (registry.all_of<ResourcePatch>(ent)) {
        auto& c = registry.get<ResourcePatch>(ent);
        comps["ResourcePatch"] = {{"patch_type", c.patch_type}, {"item_name", c.item_name}, {"obstacle", c.obstacle}, {"cell_count", static_cast<int>(c.cells.size())}};
      }
      // SpendablePool
      if (registry.all_of<SpendablePool>(ent)) {
        auto& c = registry.get<SpendablePool>(ent);
        nlohmann::json amounts_obj = nlohmann::json::object();
        for (auto& [k, v] : c.amounts) amounts_obj[k] = v;
        comps["SpendablePool"] = {{"amounts", amounts_obj}};
      }
      // proto tag
      if (registry.all_of<entt::tag<"proto"_hs>>(ent)) {
        comps["proto"] = true;
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

      // Resolve ineditor color for UI styling
      std::string ineditor_color;
      if (registry.all_of<hf::ineditor>(ent)) {
        ineditor_color = registry.get<hf::ineditor>(ent).color;
      }
      ent_json["color"] = ineditor_color;

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
    } else if (component == "ineditor" && registry.all_of<hf::ineditor>(ent)) {
      auto& c = registry.get<hf::ineditor>(ent);
      if (field == "icon") c.icon = value.get<std::string>();
      else if (field == "color") c.color = value.get<std::string>();
      else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
    } else if (component == "tags" && registry.all_of<hf::tags>(ent)) {
      auto& c = registry.get<hf::tags>(ent);
      if (field == "tags") {
        c.tags.clear();
        for (const auto& t : value) c.tags.push_back(t.get<std::string>());
      } else throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown field"};
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

  // entities.create — { name?: string, parent_entity_id?: int }
  server.router().on("entities.create", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    std::string name = params.value("name", std::string("Entity"));
    int parent_raw = params.value("parent_entity_id", -1);

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto ent = registry.create();
    hf::meta meta;
    meta.name = name;
    meta.id = fmt::format("ENT-{}", static_cast<int>(ent));
    registry.emplace<hf::meta>(ent, meta);
    registry.emplace<wl::relation>(ent);

    if (parent_raw >= 0) {
      auto parent_e = static_cast<entt::entity>(parent_raw);
      if (registry.valid(parent_e)) {
        auto& rel = registry.get<wl::relation>(ent);
        rel.parent = parent_e;
        auto& prel = registry.get_or_emplace<wl::relation>(parent_e);
        prel.children.push_back(ent);
      }
    }

    logWebAction(server, "entities.create", "ok", {{"entity_id", static_cast<int>(ent)}});
    return {{"entity_id", static_cast<int>(ent)}};
  });

  // entities.destroy — { entity_id: int }
  server.router().on("entities.destroy", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    int eid = params.at("entity_id").get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;
    auto ent = static_cast<entt::entity>(eid);
    if (!registry.valid(ent)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Invalid entity"};
    }
    auto& wk = entt::locator<WellKnownEntities>::value();
    if (isProtectedWellKnown(ent, wk)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Cannot destroy well-known entity"};
    }
    if (registry.all_of<wl::relation>(ent) && !registry.get<wl::relation>(ent).children.empty()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Entity still has children"};
    }
    removeEntityFromParentChildren(registry, ent);
    registry.destroy(ent);
    logWebAction(server, "entities.destroy", "ok", {{"entity_id", eid}});
    return {{"ok", true}};
  });

  // entities.component.add — { entity_id: int, component: string }
  server.router().on("entities.component.add", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    int eid = params.at("entity_id").get<int>();
    std::string comp = params.at("component").get<std::string>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;
    auto ent = static_cast<entt::entity>(eid);
    if (!registry.valid(ent)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Invalid entity"};
    }
    emplaceComponentByName(registry, ent, comp);
    logWebAction(server, "entities.component.add", "ok", {{"entity_id", eid}, {"component", comp}});
    return {{"ok", true}};
  });

  // entities.component.remove — { entity_id: int, component: string }
  server.router().on("entities.component.remove", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    int eid = params.at("entity_id").get<int>();
    std::string comp = params.at("component").get<std::string>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;
    auto ent = static_cast<entt::entity>(eid);
    if (!registry.valid(ent)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Invalid entity"};
    }
    removeComponentByName(registry, ent, comp);
    logWebAction(server, "entities.component.remove", "ok", {{"entity_id", eid}, {"component", comp}});
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
