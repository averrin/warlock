#include <utils/graph.hpp>

namespace graph {

void dfs(int node, const std::unordered_map<int, std::vector<int>>& adj,
         std::unordered_set<int>& visited) {
  visited.insert(node);

  for (int neighbor : adj.at(node)) {
    if (visited.find(neighbor) == visited.end()) {
      dfs(neighbor, adj, visited);
    }
  }
}

std::vector<std::vector<int>> find_connected_components(
    const std::unordered_map<int, std::vector<int>>& adj,
    const std::vector<int>& all_nodes) {
  std::unordered_set<int> visited;
  std::vector<std::vector<int>> components;

  for (const auto& [node, neighbors] : adj) {
    if (visited.find(node) == visited.end()) {
      std::unordered_set<int> net;
      dfs(node, adj, net);

      std::vector<int> component;
      for (int visited_node : net) {
        component.push_back(visited_node);
        visited.insert(visited_node);
      }

      components.push_back(component);
    }
  }

  // Add isolated nodes not present in the adjacency list
  for (int node : all_nodes) {
    if (visited.find(node) == visited.end()) {
      components.push_back({node});
    }
  }

  return components;
}

} // namespace graph
