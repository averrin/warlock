#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <game/thermal_grid.hpp>

using namespace warlock::thermal_grid;

TEST_CASE("anchorFromOccupiedCells — single cell") {
  const std::vector<std::pair<int, int>> cells = {{3, 4}};
  const auto a = anchorFromOccupiedCells(cells);
  REQUIRE(std::fabs(a.first - 3.5f) < 1e-5f);
  REQUIRE(std::fabs(a.second - 4.5f) < 1e-5f);
}

TEST_CASE("aoeFalloffWeight — edge and inside") {
  REQUIRE(aoeFalloffWeight(0.f, 5.f) == 1.f);
  REQUIRE(aoeFalloffWeight(5.f, 5.f) == 0.f);
  REQUIRE(aoeFalloffWeight(6.f, 5.f) == 0.f);
  const float mid = aoeFalloffWeight(2.5f, 5.f);
  REQUIRE(std::fabs(mid - 0.75f) < 1e-5f);
}

TEST_CASE("averageFalloffToCells — symmetric") {
  const std::vector<std::pair<int, int>> two = {{0, 0}, {1, 0}};
  const float ax = 0.5f;
  const float ay = 0.5f;
  const float w = averageFalloffToCells(ax, ay, 10.f, two);
  REQUIRE(w > 0.f);
  REQUIRE(w <= 1.f);
}
