#include <rpc/handlers/component_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <rpc/dto.hpp>
#include <game/modifier_resolve.hpp>
#include <game/connection_cleanup.hpp>
#include <game/components/items.hpp>
#include <game/frame_deposit_query.hpp>
#include <game/game_manager.hpp>
#include <game/systems/power.hpp>
#include <game/lua_completion.hpp>
#include <game/spendable.hpp>
#include <game/state.hpp>
#include <sol/sol.hpp>
#include <game/components/frame.hpp>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <magic_enum.hpp>
#include <algorithm>

namespace rpc {

namespace {

sol::object jsonArgToSol(sol::state& L, const nlohmann::json& j) {
  if (j.is_string()) return sol::make_object(L, j.get<std::string>());
  if (j.is_boolean()) return sol::make_object(L, j.get<bool>());
  if (j.is_number_integer()) return sol::make_object(L, j.get<int>());
  if (j.is_number_float()) return sol::make_object(L, j.get<double>());
  if (j.is_null()) return sol::make_object(L, sol::nil);
  return sol::make_object(L, sol::nil);
}

} // namespace

void registerComponentHandlers(Server& server) {
  // component.add — {frame_id: int, component_name: string}
  server.router().on("component.add", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("frame_id") || !params.contains("component_name")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: frame_id, component_name"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    std::string component_name = params["component_name"].get<std::string>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    // Find frame entity by data.id
    entt::entity frame_entity = entt::null;
    {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
      }
    }
    if (frame_entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }

    // Check component source exists
    if (gm.exec->sources.count(component_name) == 0) {
      throw rpc::RpcError{rpc::error::INVALID_COMPONENT, "Component source not found: " + component_name};
    }

    auto &lua = entt::locator<sol::state>::value();
    auto cost = warlock::component_script_spendable_cost(lua, gm.exec->getScript(component_name));
    std::string err;
    if (!gm.tryConsumeSpendable(cost, err)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, err};
    }

    auto& frame = registry.get<Frame>(frame_entity);
    auto component = create_component_from_lua(
        gm.exec->getState(frame.data.id),
        gm.exec->getScript(component_name));
    frame.addComponent(component);

    auto compJson = serializeComponent(*component);
    nlohmann::json result = {{"component", compJson}};
    logWebAction(server, "component.add", "ok", {{"frame_id", frame_data_id}, {"component_id", component->data.id}, {"component_name", component_name}});
    return result;
  });

  // component.activate — {frame_id: int, component_id: int}
  server.router().on("component.activate", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("frame_id") || !params.contains("component_id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: frame_id, component_id"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity frame_entity = entt::null;
    {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
      }
    }
    if (frame_entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }

    auto& frame = registry.get<Frame>(frame_entity);
    for (auto& comp : frame.components) {
      if (comp && comp->data.id == component_id) {
        const float peak = component_peak_draw_when_activating(*comp);
        if (peak > 1e-6f &&
            (!frame_on_power_network(registry, frame_data_id) ||
             !frame_network_can_afford_extra_consumption(registry, frame_data_id, peak))) {
          throw rpc::RpcError{rpc::error::INVALID_PARAMS, "insufficient power"};
        }
        if (!comp->activate()) {
          throw rpc::RpcError{rpc::error::INVALID_PARAMS, "cannot activate component"};
        }
        nlohmann::json result = {{"ok", true}};
        logWebAction(server, "component.activate", "ok", {{"frame_id", frame_data_id}, {"component_id", component_id}});
        return result;
      }
    }
    throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found"};
  });

  // component.deactivate — {frame_id: int, component_id: int}
  server.router().on("component.deactivate", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("frame_id") || !params.contains("component_id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: frame_id, component_id"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity frame_entity = entt::null;
    {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
      }
    }
    if (frame_entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }

    auto& frame = registry.get<Frame>(frame_entity);
    for (auto& comp : frame.components) {
      if (comp && comp->data.id == component_id) {
        comp->deactivate();
        nlohmann::json result = {{"ok", true}};
        logWebAction(server, "component.deactivate", "ok", {{"frame_id", frame_data_id}, {"component_id", component_id}});
        return result;
      }
    }
    throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found"};
  });

  // component.repair — {frame_id: int, component_id: int}
  server.router().on("component.repair", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("frame_id") || !params.contains("component_id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: frame_id, component_id"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity frame_entity = entt::null;
    {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
      }
    }
    if (frame_entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }

    auto& frame = registry.get<Frame>(frame_entity);
    for (auto& comp : frame.components) {
      if (comp && comp->data.id == component_id) {
        if (!comp->repair()) {
          throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Component not in repairable state"};
        }
        nlohmann::json result = {{"ok", true}};
        logWebAction(server, "component.repair", "ok", {{"frame_id", frame_data_id}, {"component_id", component_id}});
        return result;
      }
    }
    throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found"};
  });

  // component.remove — {frame_id: int, component_id: int}
  server.router().on("component.remove", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("frame_id") || !params.contains("component_id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: frame_id, component_id"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity frame_entity = entt::null;
    {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
      }
    }
    if (frame_entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }

    auto& frame = registry.get<Frame>(frame_entity);
    auto it = std::find_if(frame.components.begin(), frame.components.end(),
      [component_id](const auto& c) { return c && c->data.id == component_id; });
    if (it == frame.components.end()) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found"};
    }
    const std::string removedType = (*it)->data.get_or<std::string>("type", "");
    destroyConnectionsUsingConnectorType(registry, frame_data_id, removedType);
    frame.components.erase(it);
    nlohmann::json result = {{"ok", true}};
    logWebAction(server, "component.remove", "ok", {{"frame_id", frame_data_id}, {"component_id", component_id}});
    return result;
  });

  // component.set_size — {frame_id: int, component_id: int, size: string}
  server.router().on("component.set_size", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("frame_id") || !params.contains("component_id") || !params.contains("size")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: frame_id, component_id, size"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();
    std::string size_str = params["size"].get<std::string>();

    auto size_opt = magic_enum::enum_cast<ComponentSize>(size_str);
    if (!size_opt.has_value()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Invalid size: " + size_str};
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity frame_entity = entt::null;
    {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
      }
    }
    if (frame_entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }

    auto& frame = registry.get<Frame>(frame_entity);
    for (auto& comp : frame.components) {
      if (comp && comp->data.id == component_id) {
        comp->size = size_opt.value();
        nlohmann::json result = {{"ok", true}};
        logWebAction(server, "component.set_size", "ok", {{"frame_id", frame_data_id}, {"component_id", component_id}, {"size", size_str}});
        return result;
      }
    }
    throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found"};
  });

  // component.set_material — {frame_id: int, component_id: int, material: string}
  server.router().on("component.set_material", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("frame_id") || !params.contains("component_id") || !params.contains("material")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: frame_id, component_id, material"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();
    std::string mat_str = params["material"].get<std::string>();

    auto mat_opt = magic_enum::enum_cast<ComponentMaterial>(mat_str);
    if (!mat_opt.has_value()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Invalid material: " + mat_str};
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity frame_entity = entt::null;
    {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
      }
    }
    if (frame_entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }

    auto& frame = registry.get<Frame>(frame_entity);
    for (auto& comp : frame.components) {
      if (comp && comp->data.id == component_id) {
        comp->material = mat_opt.value();
        nlohmann::json result = {{"ok", true}};
        logWebAction(server, "component.set_material", "ok", {{"frame_id", frame_data_id}, {"component_id", component_id}, {"material", mat_str}});
        return result;
      }
    }
    throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found"};
  });

  // component.set_attribute — {frame_id: int, component_id: int, key: string, value: json}
  server.router().on("component.set_attribute", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("frame_id") || !params.contains("component_id") ||
        !params.contains("key") || !params.contains("value")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();
    std::string key = params["key"].get<std::string>();
    const nlohmann::json& value = params["value"];

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity frame_entity = entt::null;
    {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
      }
    }
    if (frame_entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }

    auto& frame = registry.get<Frame>(frame_entity);
    for (auto& comp : frame.components) {
      if (!comp || comp->data.id != component_id) continue;

      auto it = comp->data.attributes.find(key);
      if (it == comp->data.attributes.end() || !it->second) {
        throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Attribute not found: " + key};
      }
      auto& attr = *it->second;
      if (key == "recipe" && attr.GetType() == AttributeType::STRING) {
        if (!gm.items || !gm.items->loader) {
          throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Items not loaded"};
        }
        const std::string new_recipe = value.get<std::string>();
        const auto& all = gm.items->loader->get_recipes();
        auto it_r = std::find_if(all.begin(), all.end(), [&](const RecipeDefinition& r) {
          return r.name == new_recipe;
        });
        if (it_r == all.end()) {
          throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown recipe: " + new_recipe};
        }
        const auto& cn = comp->data.name;
        if (std::find(it_r->availableOn.begin(), it_r->availableOn.end(), cn) ==
            it_r->availableOn.end()) {
          throw rpc::RpcError{rpc::error::INVALID_PARAMS,
                              "Recipe is not available on " + cn};
        }
        if (cn == "Miner") {
          auto deposits = depositItemsUnderFrame(registry, frame_entity, frame);
          bool ok = false;
          for (const auto& out : it_r->outputs) {
            if (deposits.count(out.item.name)) {
              ok = true;
              break;
            }
          }
          if (!ok) {
            throw rpc::RpcError{
                rpc::error::INVALID_PARAMS,
                "Miner recipe must produce an item from a deposit under this frame"};
          }
        }
      }
      // Set value based on current attribute type
      switch (attr.GetType()) {
        case AttributeType::INT:
          attr.SetBaseValue(value.get<int>());
          break;
        case AttributeType::FLOAT:
          attr.SetBaseValue(value.get<float>());
          break;
        case AttributeType::STRING:
          attr.SetBaseValue(value.get<std::string>());
          break;
        case AttributeType::BOOL:
          attr.SetBaseValue(value.get<bool>());
          break;
      }
      if (key == "code" && gm.exec) {
        gm.exec->invalidateScript(component_id);
      }
      nlohmann::json result = {{"ok", true}};
      logWebAction(server, "component.set_attribute", "ok", {{"frame_id", frame_data_id}, {"component_id", component_id}, {"key", key}});
      return result;
    }
    throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found"};
  });

  // component.set_attribute_modifiers — {frame_id, component_id, key, modifiers: string[]}
  server.router().on("component.set_attribute_modifiers", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("frame_id") || !params.contains("component_id") ||
        !params.contains("key") || !params.contains("modifiers")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS,
                          "Missing required parameters: frame_id, component_id, key, modifiers"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();
    std::string key = params["key"].get<std::string>();
    const auto& mods_json = params["modifiers"];
    if (!mods_json.is_array()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "modifiers must be an array"};
    }
    std::vector<std::string> names;
    names.reserve(mods_json.size());
    for (const auto& el : mods_json) {
      names.push_back(el.get<std::string>());
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity frame_entity = entt::null;
    {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
      }
    }
    if (frame_entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }

    auto& frame = registry.get<Frame>(frame_entity);
    for (auto& comp : frame.components) {
      if (!comp || comp->data.id != component_id) continue;

      auto it = comp->data.attributes.find(key);
      if (it == comp->data.attributes.end() || !it->second) {
        throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Attribute not found: " + key};
      }
      auto& attr = *it->second;
      if (auto err = game::ApplyAttributeModifierNames(attr, names, gm)) {
        throw rpc::RpcError{rpc::error::INVALID_PARAMS, *err};
      }
      if (key == "code" && gm.exec) {
        gm.exec->invalidateScript(component_id);
      }
      nlohmann::json result = {{"ok", true}};
      logWebAction(server, "component.set_attribute_modifiers", "ok",
                   {{"frame_id", frame_data_id}, {"component_id", component_id}, {"key", key}});
      return result;
    }
    throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found"};
  });

  // component.call_api — {frame_id, component_id, method: string, args?: json array} -> {ok, error?}
  server.router().on("component.call_api", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started || !gm.exec) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("frame_id") || !params.contains("component_id") || !params.contains("method")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing frame_id, component_id, or method"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();
    std::string method = params["method"].get<std::string>();
    nlohmann::json args = params.value("args", nlohmann::json::array());

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    Frame* frame_ptr = nullptr;
    for (auto e : state.registry.view<Frame>()) {
      auto& f = state.registry.get<Frame>(e);
      if (f.data.id == frame_data_id) {
        frame_ptr = &f;
        break;
      }
    }
    if (!frame_ptr) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    std::shared_ptr<Component> comp;
    for (auto& c : frame_ptr->components) {
      if (c && c->data.id == component_id) {
        comp = c;
        break;
      }
    }
    if (!comp) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found"};
    }

    syncFrameLuaEnvironment(*gm.exec, frame_data_id, frame_ptr);
    refresh_component_apis(*gm.exec);

    sol::state& L = gm.exec->getState(frame_data_id);
    if (!comp->api.valid() || comp->api.get_type() != sol::type::table) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Component has no API table"};
    }
    sol::table api = comp->api;
    sol::object fn_obj = api[method];
    if (!fn_obj.valid() || !fn_obj.is<sol::function>()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "API method not found: " + method};
    }
    sol::protected_function pfn(fn_obj.as<sol::function>());
    sol::protected_function_result pr;
    if (!args.is_array()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "args must be a JSON array"};
    }
    const size_t n = args.size();
    if (n == 0) {
      pr = pfn(frame_ptr);
    } else if (n == 1) {
      pr = pfn(frame_ptr, jsonArgToSol(L, args[0]));
    } else if (n == 2) {
      pr = pfn(frame_ptr, jsonArgToSol(L, args[0]), jsonArgToSol(L, args[1]));
    } else if (n == 3) {
      pr = pfn(frame_ptr, jsonArgToSol(L, args[0]), jsonArgToSol(L, args[1]), jsonArgToSol(L, args[2]));
    } else if (n == 4) {
      pr = pfn(frame_ptr, jsonArgToSol(L, args[0]), jsonArgToSol(L, args[1]), jsonArgToSol(L, args[2]),
               jsonArgToSol(L, args[3]));
    } else {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "At most 4 API arguments supported"};
    }

    if (!pr.valid()) {
      sol::error err = pr;
      nlohmann::json result = {{"ok", false}, {"error", err.what()}};
      logWebAction(server, "component.call_api", "error",
                   {{"frame_id", frame_data_id}, {"component_id", component_id}, {"method", method}, {"error", err.what()}});
      return result;
    }
    logWebAction(server, "component.call_api", "ok",
                 {{"frame_id", frame_data_id}, {"component_id", component_id}, {"method", method}});
    return {{"ok", true}};
  });
}

} // namespace rpc
