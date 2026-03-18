#include <catch2/catch_test_macros.hpp>
#include <utils/graph.hpp>
#include <unordered_map>
#include <vector>
#include <algorithm>

// Helper: build adjacency map from edge list
static std::unordered_map<int, std::vector<int>> edges_to_adj(
    const std::vector<std::pair<int,int>>& edges, const std::vector<int>& nodes) {
  std::unordered_map<int, std::vector<int>> adj;
  for (int n : nodes) adj[n] = {};
  for (auto& [a, b] : edges) {
    adj[a].push_back(b);
    adj[b].push_back(a);
  }
  return adj;
}

TEST_CASE("find_connected_components — single network") {
  auto adj = edges_to_adj({{1,2},{2,3}}, {1,2,3});
  auto result = graph::find_connected_components(adj, {1, 2, 3});
  CHECK(result.size() == 1);
  CHECK(result[0].size() == 3);
}

TEST_CASE("find_connected_components — two separate networks") {
  auto adj = edges_to_adj({{1,2},{3,4}}, {1,2,3,4});
  auto result = graph::find_connected_components(adj, {1, 2, 3, 4});
  CHECK(result.size() == 2);
}

TEST_CASE("find_connected_components — isolated nodes") {
  auto adj = edges_to_adj({{1,2}}, {1,2,3,4,5});
  auto result = graph::find_connected_components(adj, {1, 2, 3, 4, 5});
  CHECK(result.size() == 4);  // {1,2}, {3}, {4}, {5}
}

TEST_CASE("find_connected_components — empty edges") {
  auto adj = edges_to_adj({}, {1,2});
  auto result = graph::find_connected_components(adj, {1, 2});
  CHECK(result.size() == 2);
}

TEST_CASE("find_connected_components — star topology") {
  auto adj = edges_to_adj({{1,2},{1,3},{1,4},{1,5}}, {1,2,3,4,5});
  auto result = graph::find_connected_components(adj, {1, 2, 3, 4, 5});
  CHECK(result.size() == 1);
  CHECK(result[0].size() == 5);
}
