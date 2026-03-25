#pragma once
#include <game/components/frame.hpp>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <string>
#include <vector>

/// World/grid helpers for component scripts (lidar scan, propulsion move, NFC peers).
class FrameWorld {
public:
  static constexpr float cellStep() { return 75.0f; }
  /// One third of canvas cell — smallest placement step (matches XS frame unit).
  static constexpr float subcellStep() { return cellStep() / 3.0f; }

  sol::table scanAdjacent(int id, sol::this_state s);
  /// Full-cell instant move (legacy / scripts).
  bool moveFrame(int id, std::string direction);
  /// Move by step_px (use subcellStep() for propulsion); speed > 0 tweens over ~0.35s/speed, speed ~0 snaps.
  /// extra_power_cost: burst draw (e.g. propulsion move); move is rejected if the grid would brown out.
  bool moveFrame(int id, std::string direction, float step_px, float speed, float extra_power_cost = 0.f);
  /// Other frames with NFC; maxDistance > 0 limits distance as maxDistance * grid cell (75px).
  std::vector<Frame> nfcFrames(int id, float maxDistance);
};
