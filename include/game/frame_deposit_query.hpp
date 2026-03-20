#pragma once

#include <cmath>
#include <entt/entt.hpp>
#include <game/components/frame.hpp>
#include <game/components/resource_patch.hpp>
#include <unordered_set>
#include <utility>
#include <vector>
#include <utils/entt_draw.hpp>

inline int frameSideInCells(FrameSize size) {
  switch (size) {
  case FrameSize::XS:
    return 1;
  case FrameSize::S:
    return 3;
  case FrameSize::M:
    return 6;
  case FrameSize::L:
    return 9;
  case FrameSize::G:
    return 12;
  }
  return 1;
}

inline std::vector<std::pair<int, int>> frameOccupiedGridCells(const wl::transform& t,
                                                               FrameSize size) {
  constexpr float kCellPx = 25.f;
  const int side = frameSideInCells(size);
  const int gx = static_cast<int>(std::floor(t.position.x / kCellPx));
  const int gy = static_cast<int>(std::floor(t.position.y / kCellPx));
  std::vector<std::pair<int, int>> out;
  out.reserve(static_cast<size_t>(side) * static_cast<size_t>(side));
  for (int dy = 0; dy < side; ++dy) {
    for (int dx = 0; dx < side; ++dx) {
      out.emplace_back(gx + dx, gy + dy);
    }
  }
  return out;
}

inline bool cellsOverlap(const std::vector<std::pair<int, int>>& a,
                         const std::vector<std::pair<int, int>>& b) {
  for (const auto& ca : a) {
    for (const auto& cb : b) {
      if (ca.first == cb.first && ca.second == cb.second) {
        return true;
      }
    }
  }
  return false;
}

inline std::unordered_set<std::string> depositItemsUnderFrame(entt::registry& reg,
                                                              entt::entity frame_e,
                                                              const Frame& frame) {
  std::unordered_set<std::string> out;
  if (!reg.valid(frame_e) || !reg.all_of<wl::transform>(frame_e)) {
    return out;
  }
  const auto& tr = reg.get<wl::transform>(frame_e);
  const auto cells = frameOccupiedGridCells(tr, frame.size);
  for (auto pe : reg.view<ResourcePatch>()) {
    const auto& patch = reg.get<ResourcePatch>(pe);
    if (patch.obstacle || patch.item_name.empty()) {
      continue;
    }
    if (cellsOverlap(cells, patch.cells)) {
      out.insert(patch.item_name);
    }
  }
  return out;
}
