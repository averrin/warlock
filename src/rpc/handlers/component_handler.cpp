#include <rpc/handlers/component_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <rpc/dto.hpp>
#include <game/connection_cleanup.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/components/frame.hpp>
#include <utils/entt.hpp>
#include <magic_enum.hpp>

namespace rpc {

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
        comp->activate();
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
      nlohmann::json result = {{"ok", true}};
      logWebAction(server, "component.set_attribute", "ok", {{"frame_id", frame_data_id}, {"component_id", component_id}, {"key", key}});
      return result;
    }
    throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found"};
  });
}

} // namespace rpc
