#include <rpc/handlers/items_handler.hpp>

#include <game/game_manager.hpp>
#include <rpc/message.hpp>
#include <utils/entt.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace rpc {

void registerItemsHandlers(Server& server) {
  // items.list — list loaded item definitions for UI dropdowns
  server.router().on("items.list", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.items || !gm.items->loader) {
      return {{"items", nlohmann::json::array()}};
    }

    const auto& item_map = gm.items->loader->get_items();
    std::vector<std::string> names;
    names.reserve(item_map.size());
    for (const auto& kv : item_map) {
      names.push_back(kv.first);
    }
    std::sort(names.begin(), names.end());

    nlohmann::json items = nlohmann::json::array();
    for (const auto& name : names) {
      // v1: stable id = name (future-proof if numeric ids are added later)
      items.push_back({{"id", name}, {"name", name}});
    }

    return {{"items", items}};
  });

  // recipes.list — recipe catalog for UI selectors
  server.router().on("recipes.list", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!gm.items || !gm.items->loader) {
      return {{"recipes", nlohmann::json::array()}};
    }
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& r : gm.items->loader->get_recipes()) {
      nlohmann::json outs = nlohmann::json::array();
      for (const auto& o : r.outputs) {
        outs.push_back({{"name", o.item.name}, {"amount", o.amount}});
      }
      nlohmann::json ins = nlohmann::json::array();
      for (const auto& i : r.inputs) {
        ins.push_back({{"name", i.item.name}, {"amount", i.amount}});
      }
      nlohmann::json av = nlohmann::json::array();
      for (const auto& a : r.availableOn) {
        av.push_back(a);
      }
      arr.push_back({{"name", r.name},
                     {"power_cost", r.powerCost},
                     {"available", av},
                     {"outputs", outs},
                     {"inputs", ins}});
    }
    return {{"recipes", arr}};
  });
}

} // namespace rpc
