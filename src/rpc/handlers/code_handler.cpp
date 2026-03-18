#include <rpc/handlers/code_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/components/frame.hpp>
#include <utils/entt.hpp>

namespace rpc {

void registerCodeHandlers(Server& server) {
  // code.sources — returns {sources: {name: code_string}}
  server.router().on("code.sources", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    nlohmann::json sources = nlohmann::json::object();
    for (auto& [name, code] : gm.exec->sources) {
      sources[name] = code;
    }
    return {{"sources", sources}};
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
      if (!comp) continue;
      // Find Core component(s) by type attribute
      std::string comp_type = "";
      auto it = comp->data.attributes.find("type");
      if (it != comp->data.attributes.end() && it->second) {
        auto fv = it->second->GetFinalValue();
        if (std::holds_alternative<std::string>(fv)) {
          comp_type = std::get<std::string>(fv);
        }
      }
      if (comp_type == "Core" || comp->data.name == "Core") {
        comp->data.set("code", code);
        gm.exec->invalidateScript(comp->data.id);
      }
    }
    return {{"ok", true}};
  });
}

} // namespace rpc
