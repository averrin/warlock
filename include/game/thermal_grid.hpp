#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace warlock::thermal_grid {

/// World grid cell size in pixels (must match `frame_deposit_query.hpp` / web `CELL`).
inline constexpr float kCellPx = 25.f;

inline float cellCenterX(int gx) { return static_cast<float>(gx) + 0.5f; }

inline float cellCenterY(int gy) { return static_cast<float>(gy) + 0.5f; }

/// Centroid of occupied cell centers (for AoE anchor).
inline std::pair<float, float> anchorFromOccupiedCells(
    const std::vector<std::pair<int, int>> &cells) {
  if (cells.empty()) {
    return {0.f, 0.f};
  }
  double sx = 0.0;
  double sy = 0.0;
  for (const auto &c : cells) {
    sx += static_cast<double>(cellCenterX(c.first));
    sy += static_cast<double>(cellCenterY(c.second));
  }
  const double n = static_cast<double>(cells.size());
  return {static_cast<float>(sx / n), static_cast<float>(sy / n)};
}

inline float euclidean(float ax, float ay, float bx, float by) {
  const float dx = ax - bx;
  const float dy = ay - by;
  return std::sqrt(dx * dx + dy * dy);
}

/// Quadratic falloff: 1 at d=0, 0 at d>=R.
inline float aoeFalloffWeight(float d, float R) {
  if (R <= 0.f || d > R) {
    return 0.f;
  }
  const float t = d / R;
  return std::max(0.f, 1.f - t * t);
}

inline float falloffAtCell(float ax, float ay, float R, int gx, int gy) {
  const float d = euclidean(ax, ay, cellCenterX(gx), cellCenterY(gy));
  return aoeFalloffWeight(d, R);
}

/// Average falloff over target frame cells (matches per-frame bias in thermal sim).
inline float averageFalloffToCells(float ax, float ay, float R,
                                   const std::vector<std::pair<int, int>> &targetCells) {
  if (targetCells.empty()) {
    return 0.f;
  }
  float s = 0.f;
  for (const auto &c : targetCells) {
    const float cx = cellCenterX(c.first);
    const float cy = cellCenterY(c.second);
    const float d = euclidean(ax, ay, cx, cy);
    s += aoeFalloffWeight(d, R);
  }
  return s / static_cast<float>(targetCells.size());
}

/// Stub for future per-cell airflow; returns global env airflow.
inline float effectiveAirflow(int /*gx*/, int /*gy*/, float envAirflow) {
  return envAirflow;
}

} // namespace warlock::thermal_grid
