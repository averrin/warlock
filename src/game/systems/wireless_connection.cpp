#include <game/systems/wireless_connection.hpp>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <game/game_manager.hpp>
#include <utils/entt.hpp>
#include <cmath>
#include <vector>

void WirelessConnectionSystem::fixedUpdate() {
  auto &current_state = entt::locator<State>::value();
  auto &registry = current_state.registry;
  auto &gm = entt::locator<GameManager>::value();

  constexpr float CELL = 75.0f; // Must match web canvas grid units

  auto frameCells = [](FrameSize size) -> float {
    switch (size) {
    case FrameSize::XS: return 1.0f / 3.0f;
    case FrameSize::S: return 1.0f;
    case FrameSize::M: return 2.0f;
    case FrameSize::L: return 3.0f;
    case FrameSize::G: return 4.0f;
    }
    return 1.0f;
  };

  struct EmitterInfo {
    int frame_id;
    float x, y;
    int max_connections;
    ConnectionType type;
    float radius;
  };

  struct ReceiverInfo {
    int frame_id;
    float x, y;
    int max_connections;
    ConnectionType type;
  };

  std::vector<EmitterInfo> emitters;
  std::vector<ReceiverInfo> receivers;

  for (auto entity : registry.view<Frame, wl::transform>()) {
    auto &frame = registry.get<Frame>(entity);
    const auto &t = registry.get<wl::transform>(entity);
    const float square = frameCells(frame.size) * CELL;
    const float cx = t.position.x + square / 2.0f;
    const float cy = t.position.y + square / 2.0f;

    for (const auto &c : frame.components) {
      if (c->state != ComponentState::ACTIVE) continue;

      auto compType = c->data.get_or<std::string>("type", "");
      if (compType == "Power Wireless Emitter") {
        emitters.push_back({frame.data.id, cx, cy, c->data.get_or<int>("max_connections", 10), ConnectionType::POWER, c->data.get_or<float>("radius", 10.0f) * CELL});
      } else if (compType == "Power Wireless Receiver") {
        receivers.push_back({frame.data.id, cx, cy, c->data.get_or<int>("max_connections", 1), ConnectionType::POWER});
      } else if (compType == "Data Wireless Emitter") {
        emitters.push_back({frame.data.id, cx, cy, c->data.get_or<int>("max_connections", 10), ConnectionType::DATA, c->data.get_or<float>("radius", 10.0f) * CELL});
      } else if (compType == "Data Wireless Receiver") {
        receivers.push_back({frame.data.id, cx, cy, c->data.get_or<int>("max_connections", 1), ConnectionType::DATA});
      }
    }
  }

  // Count existing wireless connections per frame per type
  std::unordered_map<int, std::unordered_map<ConnectionType, int>> emitter_conn_count;
  std::unordered_map<int, std::unordered_map<ConnectionType, int>> receiver_conn_count;

  std::vector<entt::entity> conns_to_remove;

  for (auto e : registry.view<Connection>()) {
    auto &c = registry.get<Connection>(e);
    if (c.medium != ConnectionMedium::WIRELESS) continue;

    bool valid = false;
    for (const auto &emitter : emitters) {
      if (emitter.type != c.type) continue;
      for (const auto &receiver : receivers) {
        if (receiver.type != c.type) continue;

        if ((c.source == emitter.frame_id && c.target == receiver.frame_id) ||
            (c.target == emitter.frame_id && c.source == receiver.frame_id)) {

          float dx = emitter.x - receiver.x;
          float dy = emitter.y - receiver.y;
          if (std::hypot(dx, dy) <= emitter.radius) {
            valid = true;
            emitter_conn_count[emitter.frame_id][c.type]++;
            receiver_conn_count[receiver.frame_id][c.type]++;
            break;
          }
        }
      }
      if (valid) break;
    }

    if (!valid) {
      conns_to_remove.push_back(e);
    }
  }

  for (auto e : conns_to_remove) {
    registry.destroy(e);
  }

  // Create new connections if within radius and not exceeding max connections
  for (const auto &emitter : emitters) {
    for (const auto &receiver : receivers) {
      if (emitter.type != receiver.type) continue;
      if (emitter.frame_id == receiver.frame_id) continue;

      float dx = emitter.x - receiver.x;
      float dy = emitter.y - receiver.y;

      if (std::hypot(dx, dy) <= emitter.radius) {
        // Check if connection already exists
        bool exists = false;
        for (auto e : registry.view<Connection>()) {
          auto &c = registry.get<Connection>(e);
          if (c.medium == ConnectionMedium::WIRELESS && c.type == emitter.type) {
            if ((c.source == emitter.frame_id && c.target == receiver.frame_id) ||
                (c.target == emitter.frame_id && c.source == receiver.frame_id)) {
              exists = true;
              break;
            }
          }
        }

        if (!exists) {
          if (emitter_conn_count[emitter.frame_id][emitter.type] < emitter.max_connections &&
              receiver_conn_count[receiver.frame_id][receiver.type] < receiver.max_connections) {
            gm.addConnection(emitter.frame_id, receiver.frame_id, emitter.type, ConnectionMedium::WIRELESS);
            emitter_conn_count[emitter.frame_id][emitter.type]++;
            receiver_conn_count[receiver.frame_id][receiver.type]++;
          }
        }
      }
    }
  }
}
