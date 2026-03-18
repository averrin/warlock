#include <rpc/handlers/frame_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <rpc/dto.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/well_known_entities.hpp>
#include <magic_enum.hpp>

namespace rpc {

void registerFrameHandlers(Server& server) {
  // frame.list — list all frames with full FrameDTO
  server.router().on("frame.list", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    nlohmann::json frames = nlohmann::json::array();
    auto view = registry.view<Frame>();
    for (auto entity : view) {
      auto& frame = view.get<Frame>(entity);
      frames.push_back(serializeFrame(registry, entity, frame));
    }
    return {{"frames", frames}};
  });

  // frame.get — get single frame by frame.data.id
  server.router().on("frame.get", [](const Context& /*ctx*/, const nlohmann::json& params) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: id"};
    }
    int frame_data_id = params["id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto findFrameEntity = [&](int fid) -> entt::entity {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == fid) return e;
      }
      return entt::null;
    };

    auto entity = findFrameEntity(frame_data_id);
    if (entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }

    auto& frame = registry.get<Frame>(entity);
    return {{"frame", serializeFrame(registry, entity, frame)}};
  });

  // frame.create — synchronous, returns full FrameDTO
  server.router().on("frame.create", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("name")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: name"};
    }
    std::string name = params["name"].get<std::string>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto entity = gm.addFrame(name);

    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    // Apply optional position
    if (params.contains("position") && registry.all_of<wl::transform>(entity)) {
      auto& t = registry.get<wl::transform>(entity);
      if (params["position"].contains("x")) t.position.x = params["position"]["x"].get<float>();
      if (params["position"].contains("y")) t.position.y = params["position"]["y"].get<float>();
      registry.replace<wl::transform>(entity, t);
    }

    // Apply optional size
    if (params.contains("size") && params["size"].is_string()) {
      auto& frame = registry.get<Frame>(entity);
      auto sz_str = params["size"].get<std::string>();
      auto sz_opt = magic_enum::enum_cast<FrameSize>(sz_str);
      if (sz_opt.has_value()) {
        frame.size = sz_opt.value();
        registry.replace<Frame>(entity, frame);
      }
    }

    auto& frame = registry.get<Frame>(entity);
    return {{"frame", serializeFrame(registry, entity, frame)}};
  });

  // frame.create_from_blueprint — takes {blueprint: string}
  server.router().on("frame.create_from_blueprint", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("blueprint")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: blueprint"};
    }
    std::string bp = params["blueprint"].get<std::string>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);

    // Check blueprint exists
    if (gm.exec->blueprints.count(bp) == 0) {
      throw rpc::RpcError{rpc::error::INVALID_COMPONENT, "Blueprint not found: " + bp};
    }

    int frame_data_id = gm.addFrameFromBlueprint(bp);
    if (frame_data_id == -1) {
      throw rpc::RpcError{rpc::error::INVALID_COMPONENT, "Blueprint not found: " + bp};
    }

    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto findFrameEntity = [&](int fid) -> entt::entity {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == fid) return e;
      }
      return entt::null;
    };

    auto entity = findFrameEntity(frame_data_id);
    if (entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame entity not found after blueprint creation"};
    }
    auto& frame = registry.get<Frame>(entity);
    return {{"frame", serializeFrame(registry, entity, frame)}};
  });

  // frame.remove — destroys frame entity
  server.router().on("frame.remove", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: id"};
    }
    int frame_data_id = params["id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto findFrameEntity = [&](int fid) -> entt::entity {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == fid) return e;
      }
      return entt::null;
    };

    auto entity = findFrameEntity(frame_data_id);
    if (entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    registry.destroy(entity);
    return {{"ok", true}};
  });

  // frame.move — update transform position
  server.router().on("frame.move", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id") || !params.contains("position")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: id, position"};
    }
    int frame_data_id = params["id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto findFrameEntity = [&](int fid) -> entt::entity {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == fid) return e;
      }
      return entt::null;
    };

    auto entity = findFrameEntity(frame_data_id);
    if (entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    if (!registry.all_of<wl::transform>(entity)) {
      registry.emplace<wl::transform>(entity);
    }
    auto& t = registry.get<wl::transform>(entity);
    if (params["position"].contains("x")) t.position.x = params["position"]["x"].get<float>();
    if (params["position"].contains("y")) t.position.y = params["position"]["y"].get<float>();
    registry.replace<wl::transform>(entity, t);
    return {{"ok", true}};
  });

  // frame.activate — set non-passive components to COMP_ACTIVATING
  server.router().on("frame.activate", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: id"};
    }
    int frame_data_id = params["id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto findFrameEntity = [&](int fid) -> entt::entity {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == fid) return e;
      }
      return entt::null;
    };

    auto entity = findFrameEntity(frame_data_id);
    if (entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    auto& frame = registry.get<Frame>(entity);
    for (auto& comp : frame.components) {
      if (!comp) continue;
      if (comp->data.attributes.count("passive") > 0) {
        auto val = comp->data.attributes.at("passive")->GetFinalValue();
        if (std::holds_alternative<bool>(val) && std::get<bool>(val)) continue;
      }
      comp->state = ComponentState::ACTIVATING;
    }
    return {{"ok", true}};
  });

  // frame.deactivate — set non-passive components to DEACTIVATING
  server.router().on("frame.deactivate", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: id"};
    }
    int frame_data_id = params["id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto findFrameEntity = [&](int fid) -> entt::entity {
      auto view = registry.view<Frame>();
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == fid) return e;
      }
      return entt::null;
    };

    auto entity = findFrameEntity(frame_data_id);
    if (entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    auto& frame = registry.get<Frame>(entity);
    for (auto& comp : frame.components) {
      if (!comp) continue;
      if (comp->data.attributes.count("passive") > 0) {
        auto val = comp->data.attributes.at("passive")->GetFinalValue();
        if (std::holds_alternative<bool>(val) && std::get<bool>(val)) continue;
      }
      comp->state = ComponentState::DEACTIVATING;
    }
    return {{"ok", true}};
  });

  // frame.update — update frame attributes (legacy)
  server.router().on("frame.update", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: id"};
    }
    int frame_data_id = params["id"].get<int>();

    nlohmann::json attributes = nlohmann::json::object();
    if (params.contains("attributes")) {
      attributes = params["attributes"];
    }

    gm.enqueueCommand([&gm, frame_data_id, attributes]() {
      std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
      auto& state = entt::locator<State>::value();
      auto& registry = state.registry;

      auto view = registry.view<Frame>();
      entt::entity entity = entt::null;
      for (auto e : view) {
        if (view.get<Frame>(e).data.id == frame_data_id) { entity = e; break; }
      }
      if (entity == entt::null) return;

      auto& frame = registry.get<Frame>(entity);
      if (attributes.contains("name") && attributes["name"].is_string()) {
        frame.data.name = attributes["name"].get<std::string>();
      }
      if (attributes.contains("description") && attributes["description"].is_string()) {
        frame.data.description = attributes["description"].get<std::string>();
      }
      registry.replace<Frame>(entity, frame);
    });

    return {{"status", "queued"}, {"id", frame_data_id}};
  });
}

} // namespace rpc
