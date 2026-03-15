#include <game/oracle.hpp>
#include <game/state.hpp>
#include <utils/graph.hpp>

std::vector<Frame> Oracle::getNFCFrames(int id = 0, float distance = 1.0f) {
  auto &current_state = entt::locator<State>::value();

  std::vector<Frame> frames = {};
  for (auto &f : current_state.registry.view<Frame>()) {
    auto &frame = current_state.registry.get<Frame>(f);
    if (frame.data.id == id)
      continue;
    if (!frame.hasComponentType("NFC", true))
      continue;
    frames.push_back(frame);
  }
  return frames;
}

std::vector<std::vector<int>>
findUnconnectedNets(const std::vector<Connection> &connections) {
  std::unordered_map<int, std::vector<int>> adj;
  for (const auto &connection : connections) {
    adj[connection.source].push_back(connection.target);
    adj[connection.target].push_back(connection.source);
  }
  return graph::find_connected_components(adj, {});
}

std::vector<Frame> Oracle::getWiredFrames(int id = 0) {
  auto &current_state = entt::locator<State>::value();
  auto frames_view = current_state.registry.view<Frame>();

  auto conns = std::vector<Connection>{};
  for (auto &c : current_state.registry.view<Connection>()) {
    auto conn = current_state.registry.get<Connection>(c);
    if (conn.type != ConnectionType::DATA)
      continue;
    if (conn.source != id && conn.target != id)
      continue;
    auto n = 0;
    for (auto &f : frames_view) {
      auto &frame = current_state.registry.get<Frame>(f);
      if (frame.data.id == conn.source || frame.data.id == conn.target) {
        if (frame.hasComponentType("Data Connector")) {
          n++;
        }
      }
    }
    if (n == 2) {
      conns.push_back(conn);
    }
  }

  std::vector<Frame> frames = {};
  auto nets = findUnconnectedNets(conns);
  if (nets.empty())
    return frames;

  for (auto &f : current_state.registry.view<Frame>()) {
    auto &frame = current_state.registry.get<Frame>(f);
    if (frame.data.id == id)
      continue;
    if (std::ranges::find(nets[0], frame.data.id) == nets[0].end())
      continue;
    frames.push_back(frame);
  }
  return frames;
}
