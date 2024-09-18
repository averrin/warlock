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
using Random = effolkronium::random_static;
using tweeny::easing;

// DFS function to traverse the graph
void dfs(int node, const std::unordered_map<int, std::vector<int>> &graph,
         std::unordered_set<int> &visited) {
  visited.insert(node);

  // Traverse all neighbors of the current node
  for (int neighbor : graph.at(node)) {
    if (visited.find(neighbor) == visited.end()) {
      dfs(neighbor, graph, visited);
    }
  }
}

std::vector<std::vector<int>>
findUnconnectedNets(const std::vector<Connection> &connections) {
  // Step 1: Create adjacency list for the graph
  std::unordered_map<int, std::vector<int>> graph;

  for (const auto &connection : connections) {
    graph[connection.source].push_back(connection.target);
    graph[connection.target].push_back(connection.source);
  }

  // Step 2: Find all unconnected components using flood fill (DFS)
  std::unordered_set<int> visited;
  std::vector<std::vector<int>> components;

  for (const auto &node_neighbors : graph) {
    int node = node_neighbors.first;

    if (visited.find(node) == visited.end()) {
      std::vector<int> component;
      std::unordered_set<int> net;
      dfs(node, graph, net);

      for (int visited_node : net) {
        component.push_back(visited_node);
        visited.insert(visited_node);
      }

      components.push_back(component);
    }
  }

  return components;
}

void PowerSystem::update(std::chrono::duration<double, std::milli> delta) {
  // fmt::print("PowerSystem update: {}\n", delta.count());
  // lastUpdate += delta.count();
  // if (lastUpdate < 500) {
  //   return;
  // } else {
  //   lastUpdate = 0;
  // }

  auto &info = entt::locator<PowerInfo>::emplace();

  auto &current_state = entt::locator<State>::value();

  auto conns = std::vector<Connection>{};
  for (auto &c : current_state.registry.view<Connection>()) {
    conns.push_back(current_state.registry.get<Connection>(c));
  }
  auto nets = findUnconnectedNets(conns);

  for (auto &net : nets) {

    NetworkInfo netInfo;

    auto frames = std::vector<Frame>{};
    for (auto &f : net) {
      auto &frame = current_state.registry.get<Frame>((entt::entity)f);
      frames.push_back(frame);
    }

    /*
    auto joined_view =
        frames | std::views::transform([](Frame n) { return n.data.name; });
    netInfo.data.name = fmt::join(joined_view, " -> ");
    */
    netInfo.data.name = fmt::format("Network {}", info.networks.size());

    auto production = 0.f;
    auto consumption = 0;
    auto accumulated = 0;
    auto accumulated_available = 0;
    auto battery_count = 0;
    for (auto &f : frames) {
      for (auto &c : f.components) {
        if (std::ranges::find_if(c->data.attributes, [](auto &a) {
              return a.first == "production";
            }) != c->data.attributes.end()) {

          if (tweens.find(c->data.id) == tweens.end()) {
            auto p = c->data.get<float>("production");
            auto t = tweeny::from(p * 0.8f)
                         .to(p * 1.f)
                         .during(50)
                         .via(easing::linear)
                         .from(p * 1.0f)
                         .to(p * 0.8f)
                         .during(50)
                         .via(easing::linear);
            tweens[c->data.id] = std::make_shared<tweeny::tween<float>>(t);
            // tweens[c->data.id].seek(
            //     Random::get<float>(0, 1)); // Randomize the starting point
            fmt::print("Tween created for component {} at {}\n", c->data.id,
                       tweens[c->data.id]->progress());
          }
          auto p = tweens[c->data.id]->step(0.01f);
          if (tweens[c->data.id]->progress() >= 1.f) {
            tweens[c->data.id]->seek(0);
          }
          fmt::print("{} Tween progress: {}\n", c->data.id,
                     tweens[c->data.id]->progress());
          c->data.attributes["production"]->SetBaseValue(p);
          production += c->data.get<float>("production");
        }
        if (std::ranges::find_if(c->data.attributes, [](auto &a) {
              return a.first == "consumption";
            }) != c->data.attributes.end()) {
          consumption += c->data.get<int>("consumption");
        }
      }

      auto battery =
          std::ranges::find_if(f.components, [](std::shared_ptr<Component> c) {
            return c->data.get<std::string>("type") == "Battery";
          });
      if (battery != f.components.end()) {
        battery_count += 1;
        auto charge = (*battery)->data.get<int>("charge");
        auto capacity = (*battery)->data.get<int>("capacity");
        auto discharge = (*battery)->data.get<int>("discharge");

        accumulated += charge;
        accumulated_available += charge > discharge ? discharge : charge;
      }
    }

    netInfo.production = production;
    netInfo.consumption = consumption;
    netInfo.accumulated = accumulated;
    netInfo.accumulated_available = accumulated_available;
    netInfo.battery_count = battery_count;

    info.networks.push_back(netInfo);
  }
}
