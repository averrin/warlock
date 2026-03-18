#include <rpc/handlers/items_handler.hpp>

#include <game/game_manager.hpp>
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
}

} // namespace rpc
