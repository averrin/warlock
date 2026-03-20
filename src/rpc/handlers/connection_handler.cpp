#include <rpc/handlers/connection_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <rpc/dto.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/systems/power.hpp>
#include <game/components/frame.hpp>
#include <utils/entt.hpp>
#include <magic_enum.hpp>

namespace rpc {

void registerConnectionHandlers(Server& server) {
  // connection.create — {source: int, target: int, type: string}
  // source and target are frame.data.id values
  server.router().on("connection.create", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("source") || !params.contains("target") || !params.contains("type")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: source, target, type"};
    }
    int source = params["source"].get<int>();
    int target = params["target"].get<int>();
    std::string type_str = params["type"].get<std::string>();

    auto conn_type_opt = magic_enum::enum_cast<ConnectionType>(type_str);
    if (!conn_type_opt.has_value()) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Invalid connection type: " + type_str};
    }

    ConnectionMedium medium = ConnectionMedium::WIRE;
    if (params.contains("medium") && params["medium"].is_string()) {
      auto m = magic_enum::enum_cast<ConnectionMedium>(params["medium"].get<std::string>());
      if (m.has_value()) {
        medium = m.value();
      }
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto entity = gm.addConnection(source, target, conn_type_opt.value(), medium);

    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;
    auto& conn = registry.get<Connection>(entity);
    auto connJson = serializeConnection(entity, conn, gm.items.get());
    nlohmann::json result = {{"connection", connJson}};
    logWebAction(server, "connection.create", "ok", {{"id", conn.data.id}, {"source", source}, {"target", target}, {"type", type_str}});
    return result;
  });

  // connection.list — optional {type: string}
  server.router().on("connection.list", [](const Context& /*ctx*/, const nlohmann::json& params) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }

    std::optional<ConnectionType> filter_type;
    if (params.contains("type") && params["type"].is_string()) {
      auto type_opt = magic_enum::enum_cast<ConnectionType>(params["type"].get<std::string>());
      if (type_opt.has_value()) filter_type = type_opt.value();
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    nlohmann::json connections = nlohmann::json::array();
    auto view = registry.view<Connection>();
    for (auto entity : view) {
      auto& conn = view.get<Connection>(entity);
      if (filter_type.has_value() && conn.type != filter_type.value()) continue;
      connections.push_back(serializeConnection(entity, conn, gm.items.get()));
    }
    return {{"connections", connections}};
  });

  // connection.remove — {id: int} (connection data.id)
  server.router().on("connection.remove", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: id"};
    }
    int conn_data_id = params["id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity found = entt::null;
    auto view = registry.view<Connection>();
    for (auto entity : view) {
      if (view.get<Connection>(entity).data.id == conn_data_id) {
        found = entity;
        break;
      }
    }
    if (found == entt::null) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Connection not found"};
    }
    registry.destroy(found);
    nlohmann::json result = {{"ok", true}};
    logWebAction(server, "connection.remove", "ok", {{"id", conn_data_id}});
    return result;
  });

  // power.networks — returns power network data from PowerInfo
  server.router().on("power.networks", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);

    nlohmann::json networks = nlohmann::json::array();

    // PowerInfo is emplace'd each tick by PowerSystem
    if (entt::locator<PowerInfo>::has_value()) {
      auto& info = entt::locator<PowerInfo>::value();
      for (auto& net : info.networks) {
        nlohmann::json frames_arr = nlohmann::json::array();
        for (auto f : net.frames) frames_arr.push_back(f);

        nlohmann::json history_obj = nlohmann::json::object();
        for (auto& [k, dq] : net.history) {
          nlohmann::json h = nlohmann::json::array();
          for (auto v : dq) h.push_back(v);
          history_obj[k] = h;
        }

        networks.push_back({
          {"name", net.data.name},
          {"frames", frames_arr},
          {"production", net.production},
          {"consumption", net.consumption},
          {"accumulated", net.accumulated},
          {"accumulated_available", net.accumulated_available},
          {"battery_count", net.battery_count},
          {"history", history_obj}
        });
      }
    }

    return {{"networks", networks}};
  });
}

} // namespace rpc
