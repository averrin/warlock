#include <game/oracle.hpp>
#include <game/state.hpp>
#include <utils/graph.hpp>
#include <utils/entt_draw.hpp>
#include <algorithm>
#include <cmath>

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
    int n = 0;
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

  const std::vector<int> *net = nullptr;
  for (auto &n : nets) {
    if (std::find(n.begin(), n.end(), id) != n.end()) {
      net = &n;
      break;
    }
  }
  if (!net)
    return frames;

  for (auto &f : current_state.registry.view<Frame>()) {
    auto &frame = current_state.registry.get<Frame>(f);
    if (frame.data.id == id)
      continue;
    if (std::find(net->begin(), net->end(), frame.data.id) == net->end())
      continue;
    frames.push_back(frame);
  }
  return frames;
}
