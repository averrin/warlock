#include <game/oracle.hpp>
#include <game/components/resource_patch.hpp>
#include <game/state.hpp>
#include <utils/graph.hpp>
#include <utils/entt_draw.hpp>
#include <algorithm>
#include <cmath>

namespace {
  constexpr float CELL = 75.0f; // Must match the web canvas grid units.

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

  bool isCellBlocked(entt::registry& registry, int exclude_id, int cell_x, int cell_y) {
    // Check frames
    for (auto entity : registry.view<Frame, wl::transform>()) {
      auto &frame = registry.get<Frame>(entity);
      if (frame.data.id == exclude_id) continue;

      const auto &t = registry.get<wl::transform>(entity);
      float cells = getFrameCells(frame.size);
      int fx = std::round(t.position.x / CELL);
      int fy = std::round(t.position.y / CELL);
      int f_cells = std::max(1, static_cast<int>(std::round(cells)));

      if (cell_x >= fx && cell_x < fx + f_cells &&
          cell_y >= fy && cell_y < fy + f_cells) {
        return true;
      }
    }

    // Check obstacle patches
    for (auto entity : registry.view<ResourcePatch>()) {
      auto &patch = registry.get<ResourcePatch>(entity);
      if (patch.obstacle) {
        if (patch.containsCell(cell_x, cell_y)) {
          return true;
        }
      }
    }

    return false;
  }
}

sol::table Oracle::scanAdjacent(int id, sol::this_state s) {
  sol::state_view lua(s);
  sol::table result = lua.create_table();

  auto &current_state = entt::locator<State>::value();
  auto &registry = current_state.registry;

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

  auto &frame = registry.get<Frame>(source_entity);
  auto &t = registry.get<wl::transform>(source_entity);

  float cells = getFrameCells(frame.size);
  int fx = std::round(t.position.x / CELL);
  int fy = std::round(t.position.y / CELL);
  int f_cells = std::max(1, static_cast<int>(std::round(cells)));

  bool blocked_up = false;
  bool blocked_down = false;
  bool blocked_left = false;
  bool blocked_right = false;

  for (int i = 0; i < f_cells; ++i) {
    if (isCellBlocked(registry, id, fx + i, fy - 1)) blocked_up = true;
    if (isCellBlocked(registry, id, fx + i, fy + f_cells)) blocked_down = true;
    if (isCellBlocked(registry, id, fx - 1, fy + i)) blocked_left = true;
    if (isCellBlocked(registry, id, fx + f_cells, fy + i)) blocked_right = true;
  }

  result["up"] = blocked_up;
  result["down"] = blocked_down;
  result["left"] = blocked_left;
  result["right"] = blocked_right;

  return result;
}

bool Oracle::moveFrame(int id, std::string direction) {
  auto &current_state = entt::locator<State>::value();
  auto &registry = current_state.registry;

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

  auto &frame = registry.get<Frame>(source_entity);
  auto &t = registry.get<wl::transform>(source_entity);

  float cells = getFrameCells(frame.size);
  int fx = std::round(t.position.x / CELL);
  int fy = std::round(t.position.y / CELL);
  int f_cells = std::max(1, static_cast<int>(std::round(cells)));

  bool blocked = false;
  if (direction == "up") {
    for (int i = 0; i < f_cells; ++i) {
      if (isCellBlocked(registry, id, fx + i, fy - 1)) blocked = true;
    }
    if (!blocked) t.position.y -= CELL;
  } else if (direction == "down") {
    for (int i = 0; i < f_cells; ++i) {
      if (isCellBlocked(registry, id, fx + i, fy + f_cells)) blocked = true;
    }
    if (!blocked) t.position.y += CELL;
  } else if (direction == "left") {
    for (int i = 0; i < f_cells; ++i) {
      if (isCellBlocked(registry, id, fx - 1, fy + i)) blocked = true;
    }
    if (!blocked) t.position.x -= CELL;
  } else if (direction == "right") {
    for (int i = 0; i < f_cells; ++i) {
      if (isCellBlocked(registry, id, fx + f_cells, fy + i)) blocked = true;
    }
    if (!blocked) t.position.x += CELL;
  } else {
    return false;
  }

  if (!blocked) {
    registry.patch<wl::transform>(source_entity);
  }

  return !blocked;
}

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
