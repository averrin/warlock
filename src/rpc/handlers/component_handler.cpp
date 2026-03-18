#include <rpc/handlers/component_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <rpc/dto.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/components/frame.hpp>
#include <utils/entt.hpp>

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

    return {{"component", serializeComponent(*component)}};
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
        comp->state = ComponentState::ACTIVATING;
        return {{"ok", true}};
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
        comp->state = ComponentState::DEACTIVATING;
        return {{"ok", true}};
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
      return {{"ok", true}};
    }
    throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found"};
  });
}

} // namespace rpc
