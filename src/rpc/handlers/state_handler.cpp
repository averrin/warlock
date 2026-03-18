#include <rpc/handlers/state_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <game/game_manager.hpp>

namespace rpc {

void registerStateHandlers(Server& server) {
  // state.save — save current state
  server.router().on("state.save", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }

    gm.enqueueCommand([&gm]() {
      gm.saveData();
    });

    return {{"status", "queued"}};
  });

  // state.load — load state from disk
  server.router().on("state.load", [&server](const Context& ctx, const nlohmann::json& /*params*/) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();

    gm.enqueueCommand([&gm]() {
      gm.loadData();
    });

    return {{"status", "queued"}};
  });

  // state.subscribe — register connection for topics
  server.router().on("state.subscribe", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    std::vector<std::string> topics;
    if (params.contains("topics") && params["topics"].is_array()) {
      for (const auto& t : params["topics"]) {
        if (t.is_string()) topics.push_back(t.get<std::string>());
      }
    }
    server.subscribe(ctx.connectionId, topics);
    return {{"ok", true}};
  });

  // state.unsubscribe — remove topics from connection's subscriptions
  server.router().on("state.unsubscribe", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    std::vector<std::string> topics;
    if (params.contains("topics") && params["topics"].is_array()) {
      for (const auto& t : params["topics"]) {
        if (t.is_string()) topics.push_back(t.get<std::string>());
      }
    }
    server.unsubscribe(ctx.connectionId, topics);
    return {{"ok", true}};
  });

  // state.snapshot — returns minimal current scene snapshot
  server.router().on("state.snapshot", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    uint64_t tick = gm.tick_count();
    return {
      {"tick", tick},
      {"texts", nlohmann::json::array()},
      {"lines", nlohmann::json::array()},
      {"sprites", nlohmann::json::array()},
      {"hitboxes", nlohmann::json::array()}
    };
  });
}

} // namespace rpc
