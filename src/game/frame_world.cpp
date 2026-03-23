#include <game/frame_world.hpp>
#include <game/components/resource_patch.hpp>
#include <game/state.hpp>
#include <utils/entt.hpp>
#include <utils/entt_draw.hpp>
#include <algorithm>
#include <cmath>

namespace {

constexpr float CELL = 75.0f;

float getFrameCells(FrameSize size) {
  switch (size) {
    case FrameSize::XS: return 1.0f / 3.0f;
    case FrameSize::S: return 1.0f;
    case FrameSize::M: return 2.0f;
    case FrameSize::L: return 3.0f;
    case FrameSize::G: return 4.0f;
  }
  return 1.0f;
}

int footprintInGrid(FrameSize size, float grid_px) {
  float w_px = getFrameCells(size) * CELL;
  return std::max(1, static_cast<int>(std::lround(w_px / grid_px)));
}

int toLargeCellIndex(int coord, float grid_px) {
  return static_cast<int>(std::floor(static_cast<float>(coord) * grid_px / CELL));
}

bool isCellBlocked(entt::registry& registry, int exclude_id, int cell_x, int cell_y, float grid_px) {
  for (auto entity : registry.view<Frame, wl::transform>()) {
    auto& frame = registry.get<Frame>(entity);
    if (frame.data.id == exclude_id) continue;

    const auto& t = registry.get<wl::transform>(entity);
    int fw = footprintInGrid(frame.size, grid_px);
    int fx = static_cast<int>(std::lround(t.position.x / grid_px));
    int fy = static_cast<int>(std::lround(t.position.y / grid_px));

    if (cell_x >= fx && cell_x < fx + fw && cell_y >= fy && cell_y < fy + fw) {
      return true;
    }
  }

  const int lx = toLargeCellIndex(cell_x, grid_px);
  const int ly = toLargeCellIndex(cell_y, grid_px);

  for (auto entity : registry.view<ResourcePatch>()) {
    auto& patch = registry.get<ResourcePatch>(entity);
    if (patch.obstacle) {
      if (patch.containsCell(lx, ly)) {
        return true;
      }
    }
  }

  return false;
}

constexpr float kMoveBaseSec = 0.35f;

void breakPropulsionOnBlocked(Frame& frame) {
  for (auto& comp : frame.components) {
    if (!comp) continue;
    if (comp->data.get_or<std::string>("type", "") != "Propulsion") continue;
    if (comp->state == ComponentState::BROKEN) return;
    const auto prev = comp->state;
    comp->state = ComponentState::BROKEN;
    comp->error = "Blocked by obstacle or frame";
    auto& emitter = entt::locator<event_emitter>::value();
    emitter.publish(component_state_changed{
        frame.data.id,
        comp->data.id,
        comp->data.name,
        static_cast<int>(prev),
        static_cast<int>(comp->state),
        comp->error,
    });
    return;
  }
}

} // namespace

sol::table FrameWorld::scanAdjacent(int id, sol::this_state s) {
  sol::state_view lua(s);
  sol::table result = lua.create_table();

  auto& current_state = entt::locator<State>::value();
  auto& registry = current_state.registry;

  entt::entity source_entity = entt::null;
  for (auto entity : registry.view<Frame>()) {
    if (registry.get<Frame>(entity).data.id == id) {
      source_entity = entity;
      break;
    }
  }

  if (source_entity == entt::null || !registry.all_of<wl::transform>(source_entity)) {
    return result;
  }

  auto& frame = registry.get<Frame>(source_entity);
  auto& t = registry.get<wl::transform>(source_entity);

  const float grid_px = CELL;
  int f_cells = footprintInGrid(frame.size, grid_px);
  int fx = static_cast<int>(std::lround(t.position.x / grid_px));
  int fy = static_cast<int>(std::lround(t.position.y / grid_px));

  bool blocked_up = false;
  bool blocked_down = false;
  bool blocked_left = false;
  bool blocked_right = false;

  for (int i = 0; i < f_cells; ++i) {
    if (isCellBlocked(registry, id, fx + i, fy - 1, grid_px)) blocked_up = true;
    if (isCellBlocked(registry, id, fx + i, fy + f_cells, grid_px)) blocked_down = true;
    if (isCellBlocked(registry, id, fx - 1, fy + i, grid_px)) blocked_left = true;
    if (isCellBlocked(registry, id, fx + f_cells, fy + i, grid_px)) blocked_right = true;
  }

  result["up"] = blocked_up;
  result["down"] = blocked_down;
  result["left"] = blocked_left;
  result["right"] = blocked_right;

  return result;
}

bool FrameWorld::moveFrame(int id, std::string direction) {
  return moveFrame(id, direction, CELL, 0.f);
}

bool FrameWorld::moveFrame(int id, std::string direction, float step_px, float speed) {
  auto& current_state = entt::locator<State>::value();
  auto& registry = current_state.registry;

  entt::entity source_entity = entt::null;
  for (auto entity : registry.view<Frame>()) {
    if (registry.get<Frame>(entity).data.id == id) {
      source_entity = entity;
      break;
    }
  }

  if (source_entity == entt::null || !registry.all_of<wl::transform>(source_entity)) {
    return false;
  }

  auto& frame = registry.get<Frame>(source_entity);
  auto& t = registry.get<wl::transform>(source_entity);

  if (frame.move_tween_active) {
    return false;
  }

  const float grid_px = (step_px < CELL * 0.5f) ? subcellStep() : CELL;
  int f_cells = footprintInGrid(frame.size, grid_px);
  int fx = static_cast<int>(std::lround(t.position.x / grid_px));
  int fy = static_cast<int>(std::lround(t.position.y / grid_px));

  float dx = 0.f;
  float dy = 0.f;
  bool blocked = false;

  if (direction == "up") {
    for (int i = 0; i < f_cells; ++i) {
      if (isCellBlocked(registry, id, fx + i, fy - 1, grid_px)) blocked = true;
    }
    if (!blocked) dy = -step_px;
  } else if (direction == "down") {
    for (int i = 0; i < f_cells; ++i) {
      if (isCellBlocked(registry, id, fx + i, fy + f_cells, grid_px)) blocked = true;
    }
    if (!blocked) dy = step_px;
  } else if (direction == "left") {
    for (int i = 0; i < f_cells; ++i) {
      if (isCellBlocked(registry, id, fx - 1, fy + i, grid_px)) blocked = true;
    }
    if (!blocked) dx = -step_px;
  } else if (direction == "right") {
    for (int i = 0; i < f_cells; ++i) {
      if (isCellBlocked(registry, id, fx + f_cells, fy + i, grid_px)) blocked = true;
    }
    if (!blocked) dx = step_px;
  } else {
    return false;
  }

  if (blocked) {
    breakPropulsionOnBlocked(frame);
    return false;
  }

  const float target_x = t.position.x + dx;
  const float target_y = t.position.y + dy;

  if (speed <= 1e-6f) {
    t.position.x = target_x;
    t.position.y = target_y;
    registry.patch<wl::transform>(source_entity);
    return true;
  }

  const float dur = kMoveBaseSec / std::max(speed, 0.05f);
  frame.move_tween_active = true;
  frame.move_tween_elapsed = 0.f;
  frame.move_tween_duration = dur;
  frame.move_tween_sx = t.position.x;
  frame.move_tween_sy = t.position.y;
  frame.move_tween_ex = target_x;
  frame.move_tween_ey = target_y;
  return true;
}

std::vector<Frame> FrameWorld::nfcFrames(int id, float maxDistance) {
  auto& current_state = entt::locator<State>::value();
  auto& registry = current_state.registry;
  std::vector<Frame> frames;

  float sx = 0.f, sy = 0.f;
  bool have_src = false;
  for (auto e : registry.view<Frame, wl::transform>()) {
    auto& f = registry.get<Frame>(e);
    if (f.data.id != id) continue;
    const auto& t = registry.get<wl::transform>(e);
    sx = t.position.x;
    sy = t.position.y;
    have_src = true;
    break;
  }

  const float limit = maxDistance > 0.f ? maxDistance * CELL : -1.f;

  for (auto e : registry.view<Frame>()) {
    auto& frame = registry.get<Frame>(e);
    if (frame.data.id == id) continue;
    if (!frame.hasComponentType("NFC", true)) continue;
    if (limit > 0.f && have_src && registry.all_of<wl::transform>(e)) {
      const auto& t = registry.get<wl::transform>(e);
      if (std::hypot(t.position.x - sx, t.position.y - sy) > limit) continue;
    }
    frames.push_back(frame);
  }
  return frames;
}
