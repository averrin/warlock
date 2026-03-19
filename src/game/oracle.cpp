#include <game/oracle.hpp>
#include <game/state.hpp>
#include <utils/graph.hpp>
#include <utils/entt_draw.hpp>
#include <cmath>

std::vector<Frame> Oracle::getWirelessDataFrames(int id = 0, float maxDistance = 0.0f) {
  auto &current_state = entt::locator<State>::value();

  constexpr float CELL = 75.0f; // Must match the web canvas grid units.
  constexpr const char* WIRELESS_DATA_TYPE = "Wireless Data Connector";

  auto frameCells = [](FrameSize size) -> float {
    switch (size) {
      case FrameSize::S: return 1.0f;
      case FrameSize::M: return 2.0f;
      case FrameSize::L: return 3.0f;
      case FrameSize::G: return 4.0f;
    }
    return 1.0f;
  };

  auto &registry = current_state.registry;
  float sourceCenterX = 0.0f;
  float sourceCenterY = 0.0f;
  bool sourceOk = false;

  // Find and validate source frame center.
  for (auto entity : registry.view<Frame, wl::transform>()) {
    auto &frame = registry.get<Frame>(entity);
    if (frame.data.id != id) continue;
    if (frame.size != FrameSize::M) return {};
    if (!frame.hasComponentType(WIRELESS_DATA_TYPE, true)) return {};

    const auto square = frameCells(frame.size) * CELL;
    const auto &t = registry.get<wl::transform>(entity);
    sourceCenterX = t.position.x + square / 2.0f;
    sourceCenterY = t.position.y + square / 2.0f;
    sourceOk = true;
    break;
  }
  if (!sourceOk) return {};

  if (maxDistance <= 0.0f) return {};

  std::vector<Frame> frames = {};
  for (auto entity : registry.view<Frame, wl::transform>()) {
    auto &frame = registry.get<Frame>(entity);
    if (frame.data.id == id) continue;
    if (frame.size != FrameSize::M) continue;
    if (!frame.hasComponentType(WIRELESS_DATA_TYPE, true)) continue;

    const auto square = frameCells(frame.size) * CELL;
    const auto &t = registry.get<wl::transform>(entity);
    const float cx = t.position.x + square / 2.0f;
    const float cy = t.position.y + square / 2.0f;

    const float dx = cx - sourceCenterX;
    const float dy = cy - sourceCenterY;
    if (std::hypot(dx, dy) <= maxDistance) {
      frames.push_back(frame);
    }
  }

  return frames;
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
