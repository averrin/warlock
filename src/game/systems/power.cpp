#include <effolkronium/random.hpp>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <game/systems/power.hpp>
#include <liblog/liblog.hpp>
#include <ranges> // For ranges
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <utils/graph.hpp>
using Random = effolkronium::random_static;
using hr_clock = std::chrono::high_resolution_clock;

std::vector<std::vector<int>>
findUnconnectedNets(const std::vector<Connection> &connections,
                    const std::vector<int> &all_nodes) {
  std::unordered_map<int, std::vector<int>> adj;
  for (const auto &connection : connections) {
    adj[connection.source].push_back(connection.target);
    adj[connection.target].push_back(connection.source);
  }
  return graph::find_connected_components(adj, all_nodes);
}

void PowerSystem::fixedUpdate() {

  auto &info = entt::locator<PowerInfo>::emplace();

  auto &current_state = entt::locator<State>::value();
  auto frames_view = current_state.registry.view<Frame>();

  auto conns = std::vector<Connection>{};
  for (auto &c : current_state.registry.view<Connection>()) {
    auto conn = current_state.registry.get<Connection>(c);
    if (conn.type != ConnectionType::POWER)
      continue;
    auto n = 0;
    for (auto &f : frames_view) {
      auto &frame = current_state.registry.get<Frame>(f);
      if (frame.data.id == conn.source || frame.data.id == conn.target) {
        if (conn.medium == ConnectionMedium::WIRELESS) {
          if (frame.hasComponentType("Power Wireless Emitter") || frame.hasComponentType("Power Wireless Receiver")) {
            n++;
          }
        } else {
          if (frame.hasComponentType("Power Wire Connector")) {
            n++;
          }
        }
      }
    }
    if (n == 2) {
      conns.push_back(conn);
    }
  }
  // Only components that appear in the power adjacency graph — omit isolated frames
  // (otherwise every unconnected frame becomes its own "network" in the UI).
  auto nets = findUnconnectedNets(conns, {});

  for (auto &net : nets) {

    NetworkInfo netInfo;
    netInfo.frames = net;

    auto frames = std::vector<Frame>{};

    for (auto &f : frames_view) {
      auto &frame = current_state.registry.get<Frame>(f);
      if (std::ranges::find(net, frame.data.id) != net.end()) {
        frames.push_back(frame);
      }
    }

    auto joined_view =
        frames | std::views::transform([](Frame n) { return n.data.id; });
    netInfo.data.name = fmt::format("Network {}", fmt::join(joined_view, "-"));
    if (history.find(netInfo.data.name) == history.end()) {
      history[netInfo.data.name] = {
          {"production", {}},
          {"total", {}},
          {"consumption", {}},
          {"accumulated", {}},
          {"accumulated_available", {}},
          {"battery_count", {}},
      };
    }
    auto &ph = history[netInfo.data.name]["production"];
    auto &ch = history[netInfo.data.name]["consumption"];
    auto &th = history[netInfo.data.name]["total"];
    // netInfo.data.name = fmt::format("Network {}", info.networks.size());

    auto production = 0.f;
    auto total = 0.f;
    auto consumption = 0;
    auto accumulated = 0;
    auto accumulated_available = 0;
    auto battery_count = 0;
    std::vector<std::shared_ptr<Component>> batteries;
    for (auto &f : frames) {
      for (auto &c : f.components) {
        if (c->state == ComponentState::DEACTIVATED ||
            c->state == ComponentState::DEACTIVATING ||
            c->state == ComponentState::DESTROYED) {
          continue;
        }
        if (std::ranges::find_if(c->data.attributes, [](auto &a) {
              return a.first == "consumption";
            }) != c->data.attributes.end()) {
          auto cons = c->data.get<float>("consumption");
          auto load = c->data.get_or<float>("load", 1.0f);
          auto eff = c->data.get_or<float>("efficiency", 1.0f);
          consumption += cons * load;
        }
        if (c->state == ComponentState::ACTIVATING) {
          if (std::ranges::find_if(c->data.attributes, [](auto &a) {
                return a.first == "activation_consumption";
              }) != c->data.attributes.end()) {
            auto cons = c->data.get<float>("activation_consumption");
            consumption += cons;
          }
        }
      }
      for (auto &c : f.components) {
        if (c->state != ComponentState::ACTIVE) {
          continue;
        }
        if (std::ranges::find_if(c->data.attributes, [](auto &a) {
              return a.first == "production";
            }) != c->data.attributes.end()) {
          auto eff = c->data.get_or<float>("load", 1.0f);
          if (c->data.get<std::string>("type") == "Solar") {
            eff = c->data.get_or<float>("efficiency", 1.0f);
          }
          production += c->data.get<float>("production") * eff;
        }

        if (c->data.get<std::string>("type") == "Battery") {
          battery_count += 1;
          batteries.push_back(c);
          c->data.set<std::string>("status", "IDLE");
          c->data.set<float>("heat", 0);
          auto charge = c->data.get<int>("charge");
          auto capacity = c->data.get<int>("capacity");
          auto discharge = c->data.get<int>("discharge");

          accumulated += charge;
          accumulated_available += charge > discharge ? discharge : charge;
        }
      }
    }

    auto delta = production - consumption;
    if (delta > 0) {
      for (auto &f : frames) {
        for (auto &c : f.components) {
          if (c->state != ComponentState::ACTIVE) {
            continue;
          }
          if (c->data.get<std::string>("type") == "Charger") {
            auto bat = c->data.get_or<int>("target", -1);
            if (bat >= 0) {
              auto speed = c->data.get<float>("charge_speed");
              auto load = c->data.get_or<float>("load", 1.0f);
              auto eff = c->data.get_or<float>("efficiency", 1.0f);
              std::shared_ptr<Component> battery = nullptr;
              for (auto &b : batteries) {
                if (b->data.id == bat) {
                  battery = b;
                  break;
                }
              }
              if (battery != nullptr) {
                auto charge = battery->data.get<int>("charge");
                auto capacity = battery->data.get<int>("capacity");

                if (charge < capacity) {
                  auto p = speed * eff * load;
                  if (p > capacity - charge) {
                    p = capacity - charge;
                  }
                  battery->data.set<int>("charge", charge + p);
                  battery->data.set<std::string>("status", "CHARGING");
                  battery->data.set<float>(
                      "heat", battery->data.get<float>("charge_heat"));
                }
              } else {
                auto prev = c->state;
                c->state = ComponentState::COMP_ERROR;
                c->error = "Incorrect or deactivated battery selected";
                auto &emitter = entt::locator<event_emitter>::value();
                emitter.publish(component_state_changed{
                  c->frame_id, c->data.id, c->data.name,
                  static_cast<int>(prev), static_cast<int>(c->state), c->error
                });
              }
            } else {
              auto prev = c->state;
              c->state = ComponentState::COMP_ERROR;
              c->error = "No battery selected";
              auto &emitter = entt::locator<event_emitter>::value();
              emitter.publish(component_state_changed{
                c->frame_id, c->data.id, c->data.name,
                static_cast<int>(prev), static_cast<int>(c->state), c->error
              });
            }
          }
        }
      }
    }

    auto empty = 0;
    if (delta < 0) {
      delta = -delta;
      while (delta > 0 && empty < battery_count) {
        auto p = delta / battery_count;
        for (auto &c : batteries) {
          auto charge = c->data.get<int>("charge");
          auto discharge = c->data.get<int>("discharge");
          if (p > discharge) {
            p = discharge;
          }
          if (p > charge) {
            p = charge;
          }
          if (p == 0) {
            empty += 1;
            continue;
          };
          c->data.set<int>("charge", charge - p);
          c->data.set<std::string>("status", "DISCHARGING");
          c->data.set<float>("heat", c->data.get<float>("discharge_heat"));
          delta -= p;
        }
      }
    }

    netInfo.production = production;
    ph.push_back(production);
    netInfo.consumption = consumption;
    ch.push_back(consumption);
    th.push_back(production + accumulated_available);
    netInfo.accumulated = accumulated;
    netInfo.accumulated_available = accumulated_available;
    netInfo.battery_count = battery_count;

    if (ph.size() > 100) {
      ph.pop_front();
      ch.pop_front();
      th.pop_front();
    }

    netInfo.history["production"] = ph;
    netInfo.history["consumption"] = ch;
    netInfo.history["total"] = th;

    if (consumption > production + accumulated_available) {
      for (auto &f : frames) {
        for (auto &c : f.components) {
          if (c->data.get_or<bool>("stable", false) ||
              c->data.get_or<bool>("passive", false)) {
            continue;
          }
          auto prev = c->state;
          c->state = ComponentState::DEACTIVATED;
          if (prev != ComponentState::DEACTIVATED) {
            c->error = "insufficient power";
            auto &emitter = entt::locator<event_emitter>::value();
            emitter.publish(component_state_changed{
              c->frame_id, c->data.id, c->data.name,
              static_cast<int>(prev), static_cast<int>(c->state), c->error
            });
          }
        }
      }
    }

    info.networks.push_back(netInfo);
  }
}

namespace {

struct NetworkPowerSnapshot {
  float production = 0.f;
  float consumption = 0.f;
  int accumulated_available = 0;
};

NetworkPowerSnapshot compute_network_metrics(const std::vector<Frame>& frames) {
  NetworkPowerSnapshot s;
  for (auto& f : frames) {
    for (auto& c : f.components) {
      if (c->state == ComponentState::DEACTIVATED ||
          c->state == ComponentState::DEACTIVATING ||
          c->state == ComponentState::DESTROYED) {
        continue;
      }
      if (std::ranges::find_if(c->data.attributes, [](auto& a) {
            return a.first == "consumption";
          }) != c->data.attributes.end()) {
        auto cons = c->data.get<float>("consumption");
        auto load = c->data.get_or<float>("load", 1.0f);
        s.consumption += cons * load;
      }
      if (c->state == ComponentState::ACTIVATING) {
        if (std::ranges::find_if(c->data.attributes, [](auto& a) {
              return a.first == "activation_consumption";
            }) != c->data.attributes.end()) {
          auto cons = c->data.get<float>("activation_consumption");
          s.consumption += cons;
        }
      }
    }
    for (auto& c : f.components) {
      if (c->state != ComponentState::ACTIVE) {
        continue;
      }
      if (std::ranges::find_if(c->data.attributes, [](auto& a) {
            return a.first == "production";
          }) != c->data.attributes.end()) {
        auto eff = c->data.get_or<float>("load", 1.0f);
        if (c->data.get<std::string>("type") == "Solar") {
          eff = c->data.get_or<float>("efficiency", 1.0f);
        }
        s.production += c->data.get<float>("production") * eff;
      }

      if (c->data.get<std::string>("type") == "Battery") {
        auto charge = c->data.get<int>("charge");
        auto discharge = c->data.get<int>("discharge");
        s.accumulated_available += charge > discharge ? discharge : charge;
      }
    }
  }
  return s;
}

} // namespace

bool frame_network_can_afford_extra_consumption(entt::registry& registry, int frame_id, float extra) {
  if (extra <= 1e-6f) return true;
  auto frames_view = registry.view<Frame>();
  auto conns = std::vector<Connection>{};
  for (auto& c : registry.view<Connection>()) {
    auto conn = registry.get<Connection>(c);
    if (conn.type != ConnectionType::POWER)
      continue;
    auto n = 0;
    for (auto& f : frames_view) {
      auto& frame = registry.get<Frame>(f);
      if (frame.data.id == conn.source || frame.data.id == conn.target) {
        if (conn.medium == ConnectionMedium::WIRELESS) {
          if (frame.hasComponentType("Power Wireless Emitter") ||
              frame.hasComponentType("Power Wireless Receiver")) {
            n++;
          }
        } else {
          if (frame.hasComponentType("Power Wire Connector")) {
            n++;
          }
        }
      }
    }
    if (n == 2) {
      conns.push_back(conn);
    }
  }
  auto nets = findUnconnectedNets(conns, {});
  std::vector<int>* found_net = nullptr;
  for (auto& net : nets) {
    if (std::ranges::find(net, frame_id) != net.end()) {
      found_net = &net;
      break;
    }
  }
  if (!found_net) return true;
  auto frames = std::vector<Frame>{};
  for (auto& f : frames_view) {
    auto& frame = registry.get<Frame>(f);
    if (std::ranges::find(*found_net, frame.data.id) != found_net->end()) {
      frames.push_back(frame);
    }
  }
  auto snap = compute_network_metrics(frames);
  return snap.consumption + extra <= snap.production + static_cast<float>(snap.accumulated_available);
}
