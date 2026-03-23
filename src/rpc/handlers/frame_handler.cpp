#include <rpc/handlers/frame_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <rpc/dto.hpp>
#include <game/game_manager.hpp>
#include <game/spendable.hpp>
#include <game/state.hpp>
#include <game/well_known_entities.hpp>
#include <magic_enum.hpp>
#include <utils/entt_lua.hpp>

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
      frames.push_back(serializeFrameSummary(entity, frame));
    }
    return {{"frames", frames}};
  });

  // frame.get — get single frame by entity_id
  server.router().on("frame.get", [](const Context& /*ctx*/, const nlohmann::json& params) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: id"};
    }
    int eid = params["id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto entity = static_cast<entt::entity>(eid);
    if (!registry.valid(entity) || !registry.all_of<Frame>(entity)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }

    auto& frame = registry.get<Frame>(entity);
    return serializeFrame(registry, entity, frame);
  });

  // frame.create — async, enqueues command and returns status
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
    nlohmann::json captured_params = params;

    gm.enqueueCommand([&gm, name, captured_params]() {
      std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
      auto entity = gm.addFrame(name);
      auto& state = entt::locator<State>::value();
      auto& registry = state.registry;

      if (captured_params.contains("position") && registry.all_of<wl::transform>(entity)) {
        auto& t = registry.get<wl::transform>(entity);
        if (captured_params["position"].contains("x")) t.position.x = captured_params["position"]["x"].get<float>();
        if (captured_params["position"].contains("y")) t.position.y = captured_params["position"]["y"].get<float>();
        registry.replace<wl::transform>(entity, t);
      }

      if (captured_params.contains("size") && captured_params["size"].is_string()) {
        auto& frame = registry.get<Frame>(entity);
        auto sz_str = captured_params["size"].get<std::string>();
        auto sz_opt = magic_enum::enum_cast<FrameSize>(sz_str);
        if (sz_opt.has_value()) {
          frame.size = sz_opt.value();
          registry.replace<Frame>(entity, frame);
        }
      }
    });

    nlohmann::json result = {{"status", "queued"}, {"name", name}};
    logWebAction(server, "frame.create", "queued", {{"name", name}});
    return result;
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

    auto &lua = entt::locator<sol::state>::value();
    std::string bp_source = gm.exec->blueprints[bp];
    sol::table bp_spec = lua.load(bp_source).call();
    auto total = warlock::blueprint_spendable_total(lua, *gm.exec, bp_spec);
    std::string err;
    if (!gm.tryConsumeSpendable(total, err)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, err};
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

    if (params.contains("position") && registry.all_of<wl::transform>(entity)) {
      auto& t = registry.get<wl::transform>(entity);
      if (params["position"].contains("x")) t.position.x = params["position"]["x"].get<float>();
      if (params["position"].contains("y")) t.position.y = params["position"]["y"].get<float>();
      registry.replace<wl::transform>(entity, t);
    }

    auto& frame = registry.get<Frame>(entity);
    auto frameJson = serializeFrame(registry, entity, frame);
    logWebAction(server, "frame.create_from_blueprint", "ok", {{"blueprint", bp}, {"frame_id", frame.data.id}});
    return frameJson;
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
    int eid = params["id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto entity = static_cast<entt::entity>(eid);
    if (!registry.valid(entity) || !registry.all_of<Frame>(entity)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    registry.destroy(entity);
    nlohmann::json result = {{"ok", true}};
    logWebAction(server, "frame.remove", "ok", {{"id", eid}});
    return result;
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
    int eid = params["id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto entity = static_cast<entt::entity>(eid);
    if (!registry.valid(entity) || !registry.all_of<Frame>(entity)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    if (!registry.all_of<wl::transform>(entity)) {
      registry.emplace<wl::transform>(entity);
    }
    auto& t = registry.get<wl::transform>(entity);
    float x = t.position.x;
    float y = t.position.y;
    if (params["position"].contains("x")) x = params["position"]["x"].get<float>();
    if (params["position"].contains("y")) y = params["position"]["y"].get<float>();
    t.position.x = x;
    t.position.y = y;
    registry.replace<wl::transform>(entity, t);
    nlohmann::json result = {{"ok", true}};
    logWebAction(server, "frame.move", "ok", {{"id", eid}, {"x", x}, {"y", y}});
    return result;
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
    int eid = params["id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto entity = static_cast<entt::entity>(eid);
    if (!registry.valid(entity) || !registry.all_of<Frame>(entity)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    auto& frame = registry.get<Frame>(entity);
    for (auto& comp : frame.components) {
      if (!comp) continue;
      auto prev = comp->state;
      if (comp->activate()) {
        logWebAction(server, "component.state", "user_activate", {
          {"frame_id", frame.data.id}, {"component_id", comp->data.id},
          {"component_name", comp->data.name},
          {"prev_state", static_cast<int>(prev)},
          {"new_state", static_cast<int>(comp->state)}
        });
      }
    }
    nlohmann::json result = {{"ok", true}};
    logWebAction(server, "frame.activate", "ok", {{"id", eid}});
    return result;
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
    int eid = params["id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto entity = static_cast<entt::entity>(eid);
    if (!registry.valid(entity) || !registry.all_of<Frame>(entity)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    auto& frame = registry.get<Frame>(entity);
    for (auto& comp : frame.components) {
      if (!comp) continue;
      auto prev = comp->state;
      if (comp->deactivate()) {
        logWebAction(server, "component.state", "user_deactivate", {
          {"frame_id", frame.data.id}, {"component_id", comp->data.id},
          {"component_name", comp->data.name},
          {"prev_state", static_cast<int>(prev)},
          {"new_state", static_cast<int>(comp->state)}
        });
      }
    }
    nlohmann::json result = {{"ok", true}};
    logWebAction(server, "frame.deactivate", "ok", {{"id", eid}});
    return result;
  });

  // frame.set_size — {id: int, size: string}
  server.router().on("frame.set_size", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id") || !params.contains("size")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: id, size"};
    }
    int eid = params["id"].get<int>();
    std::string size_str = params["size"].get<std::string>();

    auto size_opt = magic_enum::enum_cast<FrameSize>(size_str);
    if (!size_opt.has_value()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Invalid frame size: " + size_str};
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto entity = static_cast<entt::entity>(eid);
    if (!registry.valid(entity) || !registry.all_of<Frame>(entity)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    auto& frame = registry.get<Frame>(entity);
    frame.size = size_opt.value();
    nlohmann::json result = {{"ok", true}};
    logWebAction(server, "frame.set_size", "ok", {{"id", eid}, {"size", size_str}});
    return result;
  });

  // frame.set_material — {id: int, material: string}
  server.router().on("frame.set_material", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id") || !params.contains("material")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: id, material"};
    }
    int eid = params["id"].get<int>();
    std::string mat_str = params["material"].get<std::string>();

    auto mat_opt = magic_enum::enum_cast<ComponentMaterial>(mat_str);
    if (!mat_opt.has_value()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Invalid material: " + mat_str};
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto entity = static_cast<entt::entity>(eid);
    if (!registry.valid(entity) || !registry.all_of<Frame>(entity)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    auto& frame = registry.get<Frame>(entity);
    frame.material = mat_opt.value();
    nlohmann::json result = {{"ok", true}};
    logWebAction(server, "frame.set_material", "ok", {{"id", eid}, {"material", mat_str}});
    return result;
  });

  // frame.update — update frame metadata fields
  server.router().on("frame.update", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: id"};
    }
    int eid = params["id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto entity = static_cast<entt::entity>(eid);
    if (!registry.valid(entity) || !registry.all_of<Frame>(entity)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    auto& frame = registry.get<Frame>(entity);

    // Metadata fields
    if (params.contains("name") && params["name"].is_string()) {
      frame.data.name = params["name"].get<std::string>();
    }
    if (params.contains("description") && params["description"].is_string()) {
      frame.data.description = params["description"].get<std::string>();
    }
    if (params.contains("icon") && params["icon"].is_string()) {
      frame.data.icon = params["icon"].get<std::string>();
    }

    // Attribute updates: {attributes: {key: value, ...}}
    if (params.contains("attributes") && params["attributes"].is_object()) {
      for (auto& [key, val] : params["attributes"].items()) {
        auto it = frame.data.attributes.find(key);
        if (it == frame.data.attributes.end() || !it->second) {
          // Create new attribute if it doesn't exist (for player-settable properties like accent)
          if (val.is_string()) {
            frame.data.attributes[key] = std::make_shared<Attribute>(
              key, "", AttributeType::STRING, val.get<std::string>());
          } else if (val.is_number_integer()) {
            frame.data.attributes[key] = std::make_shared<Attribute>(
              key, "", AttributeType::INT, val.get<int>());
          } else if (val.is_number()) {
            frame.data.attributes[key] = std::make_shared<Attribute>(
              key, "", AttributeType::FLOAT, val.get<float>());
          } else if (val.is_boolean()) {
            frame.data.attributes[key] = std::make_shared<Attribute>(
              key, "", AttributeType::BOOL, val.get<bool>());
          }
          continue;
        }
        auto& attr = *it->second;
        switch (attr.GetType()) {
          case AttributeType::INT:
            if (val.is_number_integer()) attr.SetBaseValue(val.get<int>());
            break;
          case AttributeType::FLOAT:
            if (val.is_number()) attr.SetBaseValue(val.get<float>());
            break;
          case AttributeType::STRING:
            if (val.is_string()) attr.SetBaseValue(val.get<std::string>());
            break;
          case AttributeType::BOOL:
            if (val.is_boolean()) attr.SetBaseValue(val.get<bool>());
            break;
        }
      }
    }

    auto frameJson = serializeFrame(registry, entity, frame);
    nlohmann::json result = {{"ok", true}, {"frame", frameJson}};
    logWebAction(server, "frame.update", "ok", {{"id", eid}});
    return result;
  });
}

} // namespace rpc
