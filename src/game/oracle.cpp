#include <game/oracle.hpp>
#include <game/state.hpp>

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

// TODO: move to helpers
void dfs2(int node, const std::unordered_map<int, std::vector<int>> &graph,
          std::unordered_set<int> &visited) {
  visited.insert(node);

  for (int neighbor : graph.at(node)) {
    if (visited.find(neighbor) == visited.end()) {
      dfs2(neighbor, graph, visited);
    }
  }
}

std::vector<std::vector<int>>
findUnconnectedNets(const std::vector<Connection> &connections) {
  // Step 1: Create adjacency list for the graph
  std::unordered_map<int, std::vector<int>> graph;

  for (const auto &connection : connections) {
    graph[connection.source].push_back(connection.target);
    graph[connection.target].push_back(connection.source);
  }

  // Step 2: Find all unconnected components using flood fill (DFS)
  std::unordered_set<int> visited;
  std::vector<std::vector<int>> components;

  for (const auto &node_neighbors : graph) {
    int node = node_neighbors.first;

    if (visited.find(node) == visited.end()) {
      std::vector<int> component;
      std::unordered_set<int> net;
      dfs2(node, graph, net);

      for (int visited_node : net) {
        component.push_back(visited_node);
        visited.insert(visited_node);
      }

      components.push_back(component);
    }
  }

  return components;
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
