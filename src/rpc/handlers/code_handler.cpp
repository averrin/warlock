#include <rpc/handlers/code_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/components/frame.hpp>
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
