#include <rpc/handlers/objective_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <game/game_manager.hpp>
#include <utils/entt.hpp>

namespace rpc {

namespace {

nlohmann::json serializeObjectiveList(GameManager& gm) {
  auto& om = *gm.objectives;
  nlohmann::json items = nlohmann::json::array();
  for (const auto& [id, obj] : om.objectives()) {
    std::string status;
    if (om.isCompleted(id)) {
      status = "completed";
    } else if (!om.prerequisitesMet(id)) {
      status = "locked";
    } else if (obj.hidden && !om.isCompleted(id)) {
      status = "hidden";
    } else {
      status = "available";
    }

    nlohmann::json prereqs_j = nlohmann::json::array();
    for (const auto& p : obj.prerequisites) {
      prereqs_j.push_back(p);
    }

    nlohmann::json conditions_j = nlohmann::json::array();
    for (const auto& c : obj.conditions) {
      nlohmann::json cj = {{"type", c.type}};
      if (!c.name.empty()) cj["name"] = c.name;
      if (c.amount != 0) cj["amount"] = c.amount;
      conditions_j.push_back(cj);
    }

    nlohmann::json rewards_j = nlohmann::json::array();
    for (const auto& r : obj.rewards) {
      nlohmann::json rj = {{"type", r.type}};
      if (!r.name.empty()) rj["name"] = r.name;
      if (r.amount != 0) rj["amount"] = r.amount;
      if (!r.message.empty()) rj["message"] = r.message;
      rewards_j.push_back(rj);
    }

    items.push_back({
      {"id", id},
      {"name", obj.name},
      {"description", obj.description},
      {"icon", obj.icon},
      {"category", obj.category},
      {"prerequisites", prereqs_j},
      {"conditions", conditions_j},
      {"rewards", rewards_j},
      {"hidden", obj.hidden},
      {"repeatable", obj.repeatable},
      {"status", status},
    });
  }
  return {{"objectives", items}};
}

} // namespace

void registerObjectiveHandlers(Server& server) {
  // objectives.list — returns all objectives with their current status
  server.router().on("objectives.list", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started || !gm.objectives) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    return serializeObjectiveList(gm);
  });

  // objectives.check — {id: string} — force re-evaluate a specific objective
  server.router().on("objectives.check", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started || !gm.objectives) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id") || !params["id"].is_string()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: id"};
    }
    std::string id = params["id"].get<std::string>();
    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);

    if (!gm.objectives->objectives().count(id)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown objective: " + id};
    }

    gm.objectives->evaluate(gm);
    return serializeObjectiveList(gm);
  });

  // objectives.complete — {id: string} — debug: manually complete an objective
  server.router().on("objectives.complete", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started || !gm.objectives) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id") || !params["id"].is_string()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: id"};
    }
    std::string id = params["id"].get<std::string>();
    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);

    if (!gm.objectives->objectives().count(id)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown objective: " + id};
    }
    if (gm.objectives->isCompleted(id)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Already completed: " + id};
    }

    gm.objectives->complete(id, gm);
    gm.saveData();
    return serializeObjectiveList(gm);
  });
}

} // namespace rpc
