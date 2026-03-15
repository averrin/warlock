#pragma once
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace graph {
  void dfs(int node, const std::unordered_map<int, std::vector<int>>& adj,
           std::unordered_set<int>& visited);

  std::vector<std::vector<int>> find_connected_components(
      const std::unordered_map<int, std::vector<int>>& adj,
      const std::vector<int>& all_nodes);
}
