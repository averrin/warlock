#include <rpc/handlers/research_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <game/game_manager.hpp>
#include <utils/entt.hpp>

namespace rpc {

namespace {

nlohmann::json serializeResearchList(GameManager& gm) {
  auto& rm = *gm.research;
  nlohmann::json nodes = nlohmann::json::array();
  for (const auto& [name, node] : rm.nodes()) {
    std::string status;
    if (rm.isUnlocked(name)) {
      status = "unlocked";
    } else if (rm.prerequisitesMet(name)) {
      status = "available";
    } else {
      status = "locked";
    }

    nlohmann::json cost_j = nlohmann::json::object();
    for (const auto& [k, v] : node.cost) {
      cost_j[k] = v;
    }

    nlohmann::json requires_j = nlohmann::json::array();
    for (const auto& r : node.requires) {
      requires_j.push_back(r);
    }

    nlohmann::json unlocks_j = nlohmann::json::array();
    for (const auto& u : node.unlocks) {
      unlocks_j.push_back(u);
    }

    nodes.push_back({
      {"name", name},
      {"description", node.description},
      {"icon", node.icon},
      {"cost", cost_j},
      {"requires", requires_j},
      {"unlocks", unlocks_j},
      {"status", status},
    });
  }
  return {{"nodes", nodes}};
}

} // namespace

void registerResearchHandlers(Server& server) {
  // research.list — returns all research nodes with their current status
  server.router().on("research.list", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started || !gm.research) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    return serializeResearchList(gm);
  });

  // research.unlock — {name: string} — spend resources and unlock a research
  server.router().on("research.unlock", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started || !gm.research) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("name") || !params["name"].is_string()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: name"};
    }
    std::string name = params["name"].get<std::string>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& rm = *gm.research;

    if (!rm.nodes().count(name)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown research: " + name};
    }
    if (rm.isUnlocked(name)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Already unlocked: " + name};
    }
    if (!rm.prerequisitesMet(name)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Prerequisites not met for: " + name};
    }

    const auto& cost = rm.nodes().at(name).cost;
    std::string err;
    if (!gm.tryConsumeSpendable(cost, err)) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, err};
    }

    rm.unlock(name);
    gm.saveData();

    return serializeResearchList(gm);
  });

  // research.unlock_all — debug: unlock all research nodes without cost
  server.router().on("research.unlock_all", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started || !gm.research) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    gm.research->unlockAll();
    gm.saveData();
    return serializeResearchList(gm);
  });
}

} // namespace rpc
