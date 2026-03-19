#include <rpc/handlers/code_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/components/frame.hpp>
#include <algorithm>
#include <cctype>
#include <utils/entt.hpp>

namespace rpc {

static entt::entity findFrameEntityByDataId(entt::registry& registry, int frameDataId) {
  auto view = registry.view<Frame>();
  for (auto entity : view) {
    if (view.get<Frame>(entity).data.id == frameDataId) {
      return entity;
    }
  }
  return entt::null;
}

static std::shared_ptr<Component> findCoreComponent(Frame& frame) {
  for (auto& comp : frame.components) {
    if (!comp) {
      continue;
    }
    std::string compType = comp->data.get_or<std::string>("type", "");
    if (compType == "Core" || comp->data.name == "Core") {
      return comp;
    }
  }
  return nullptr;
}

void registerCodeHandlers(Server& server) {
  // Wire execution log callback → web.log broadcast
  auto& gm_init = entt::locator<GameManager>::value();
  if (gm_init.exec) {
    gm_init.exec->setLogCallback([&server](const std::string& source,
                                           const std::string& status,
                                           const nlohmann::json& args) {
      logWebAction(server, source, status, args);
    });
  }

  // code.sources — returns {sources: {name: code_string}}
  server.router().on("code.sources", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    nlohmann::json sources = nlohmann::json::object();
    nlohmann::json categories = nlohmann::json::object();
    for (auto& [name, code] : gm.exec->sources) {
      sources[name] = code;
      categories[name] = gm.exec->categories.count(name) ? gm.exec->categories.at(name) : "Other";
    }
    return {{"sources", sources}, {"categories", categories}};
  });

  // code.blueprints — returns {blueprints: {name: bp_string}}
  server.router().on("code.blueprints", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    nlohmann::json blueprints = nlohmann::json::object();
    for (auto& [name, bp] : gm.exec->blueprints) {
      blueprints[name] = bp;
    }
    return {{"blueprints", blueprints}};
  });

  // code.update — {frame_id: int, code: string}
  server.router().on("code.update", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("frame_id") || !params.contains("code")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: frame_id, code"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    std::string code = params["code"].get<std::string>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;
    entt::entity frame_entity = findFrameEntityByDataId(registry, frame_data_id);
    if (frame_entity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }

    auto& frame = registry.get<Frame>(frame_entity);
    auto core = findCoreComponent(frame);
    if (!core) {
      throw rpc::RpcError{rpc::error::INVALID_COMPONENT, "Core component not found"};
    }
    core->data.set("code", code);
    gm.exec->invalidateScript(core->data.id);
    nlohmann::json result = {{"ok", true}};
    logWebAction(server, "code.update", "ok", {{"frame_id", frame_data_id}});
    return result;
  });

  // code.update_source — {source_name: string, code: string}
  // Update an in-memory component definition and reapply it to existing instances.
  server.router().on("code.update_source", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("source_name") || !params.contains("code")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: source_name, code"};
    }

    std::string sourceName = params["source_name"].get<std::string>();
    std::string code = params["code"].get<std::string>();

    auto toLower = [](std::string s) {
      for (char& ch : s) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      }
      return s;
    };

    std::string wantedLower = toLower(sourceName);
    std::string canonicalName;
    for (auto& [name, _] : gm.exec->sources) {
      if (toLower(name) == wantedLower) {
        canonicalName = name;
        break;
      }
    }
    if (canonicalName.empty()) {
      for (auto& [name, _] : gm.exec->sources) {
        std::string n = toLower(name);
        if (n.find(wantedLower) != std::string::npos) {
          canonicalName = name;
          break;
        }
      }
    }
    if (canonicalName.empty()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Source not found: " + sourceName};
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    gm.exec->sources[canonicalName] = code;
    const std::string canonicalLower = toLower(canonicalName);

    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    auto frames = registry.view<Frame>();
    for (auto frameEntity : frames) {
      auto& frame = registry.get<Frame>(frameEntity);
      for (auto& compPtr : frame.components) {
        if (!compPtr) continue;
        // `source_name` is keyed by the component script spec title, but in some
        // runtime cases the instance `data.name` may differ while the `type`
        // attribute remains stable (and is what `getComponentByType()` uses).
        const std::string compType = compPtr->data.get_or<std::string>("type", "");
        const std::string instTypeLower = toLower(compType);
        const std::string instNameLower = toLower(compPtr->data.name);
        if (instTypeLower != canonicalLower && instNameLower != canonicalLower) {
          continue;
        }

        const int prev_data_id = compPtr->data.id;
        const int prev_frame_id = compPtr->frame_id;
        const ComponentState prev_state = compPtr->state;
        const int prev_time_switch = compPtr->time_switch;
        const ComponentState prev_next_state = compPtr->next_state;
        const int prev_storage_slots = compPtr->storage ? compPtr->storage->slotsCount : 0;

        auto rebuilt = create_component_from_lua(gm.exec->getState(frame.data.id), code);
        if (!rebuilt) continue;

        // Overwrite spec-derived fields, but preserve runtime identity and tween transition state.
        rebuilt->data.id = prev_data_id;
        rebuilt->frame_id = prev_frame_id;
        rebuilt->state = prev_state;
        rebuilt->time_switch = prev_time_switch;
        rebuilt->next_state = prev_next_state;

        const int new_storage_slots = rebuilt->data.get_or<int>("slots", 0);
        if (compPtr->storage && prev_storage_slots == new_storage_slots) {
          // Preserve runtime storage object when slot count matches.
          rebuilt->storage = compPtr->storage;
        } else {
          rebuilt->storage = nullptr;
        }

        // Swap the shared_ptr so Lua sees a fresh Component + api table.
        compPtr = rebuilt;

        // Core scripts are compiled/cached by component id; invalidate if a Core component was refreshed.
        const std::string typeStr = compPtr->data.get_or<std::string>("type", "");
        if (typeStr == "Core" || compPtr->data.name == "Core") {
          gm.exec->invalidateScript(compPtr->data.id);
        }
      }
    }

    nlohmann::json result = {{"ok", true}, {"source_name", canonicalName}};
    logWebAction(server, "code.update_source", "ok", {{"source_name", canonicalName}});
    return result;
  });

  // code.get_script — {frame_id: int} -> {frame_id, script}
  server.router().on("code.get_script", [](const Context& /*ctx*/, const nlohmann::json& params) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("frame_id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: frame_id"};
    }
    int frameDataId = params["frame_id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto frameEntity = findFrameEntityByDataId(state.registry, frameDataId);
    if (frameEntity == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};
    }
    auto& frame = state.registry.get<Frame>(frameEntity);
    auto core = findCoreComponent(frame);
    if (!core) {
      throw rpc::RpcError{rpc::error::INVALID_COMPONENT, "Core component not found"};
    }

    return {
      {"frame_id", frameDataId},
      {"script", core->data.get_or<std::string>("code", "")}
    };
  });

  // code.execute — {frame_id: int, function?: string} -> {status, error?}
  server.router().on("code.execute", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      return {{"status", "error"}, {"error", "Game not started"}};
    }
    if (!params.contains("frame_id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: frame_id"};
    }
    int frameDataId = params["frame_id"].get<int>();
    std::string functionName = params.value("function", std::string("update"));

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto frameEntity = findFrameEntityByDataId(state.registry, frameDataId);
    if (frameEntity == entt::null) {
      nlohmann::json result = {{"status", "error"}, {"error", "Frame not found"}};
      logWebAction(server, "code.execute", "error", {{"frame_id", frameDataId}, {"error", "Frame not found"}});
      return result;
    }
    auto& frame = state.registry.get<Frame>(frameEntity);
    auto core = findCoreComponent(frame);
    if (!core) {
      nlohmann::json result = {{"status", "error"}, {"error", "Core component not found"}};
      logWebAction(server, "code.execute", "error", {{"frame_id", frameDataId}, {"error", "Core component not found"}});
      return result;
    }

    core->error.clear();
    gm.exec->executeCoreFunction(core, functionName);
    if (core->state == ComponentState::COMP_ERROR) {
      nlohmann::json result = {{"status", "error"}, {"error", core->error}};
      logWebAction(server, "code.execute", "error", {{"frame_id", frameDataId}, {"error", core->error}});
      return result;
    }
    nlohmann::json result = {{"status", "ok"}};
    logWebAction(server, "code.execute", "ok", {{"frame_id", frameDataId}, {"function", functionName}});
    return result;
  });
}

} // namespace rpc
