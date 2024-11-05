#pragma once
#include <app/scene.hpp>
#include <deque>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <game/system.hpp>
#include <game/systems/input.hpp>
#include <utils/entt.hpp>
#include <utils/entt_draw.hpp>
#include <utils/entt_lua.hpp>
#include <vector>

class PresentationSystem : public System {
  entt::observer frame_observer;
  entt::observer connection_observer;

  entt::entity position_marker = entt::null;
  std::map<std::string, entt::entity> position_markers = {};

public:
  void fixedUpdate() override;
  void resetDrawables();

  template <typename T>
  T &makeEntity(entt::registry &registry, const std::string &name,
                entt::entity parent = entt::null) {
    auto entity = registry.create();
    registry.emplace<hf::meta>(
        entity, hf::meta{name, name, fmt::format("{}_{}", name, (int)entity)});
    if (parent != entt::null) {
      registry.emplace<wl::relation>(entity, wl::relation{{}, parent});
      auto &parent_relation = registry.get<wl::relation>(parent);
      parent_relation.children.push_back(entity);
    }
    return registry.emplace<T>(entity);
  }

  entt::entity getChildByName(entt::registry &registry, const std::string &name,
                              entt::entity parent = entt::null) {
    if (parent != entt::null) {
      auto &relation = registry.get<wl::relation>(parent);
      for (auto &child : relation.children) {
        if (!registry.all_of<hf::meta>(child)) {
          continue;
        }
        auto &meta = registry.get<hf::meta>(child);
        if (meta.name == name) {
          return child;
        }
      }
    } else {
      auto view = registry.view<entt::entity>();
      for (auto e : view) {
        if (!registry.all_of<hf::meta>(e)) {
          continue;
        }
        auto &meta = registry.get<hf::meta>(e);
        if (meta.name == name) {
          return e;
        }
      }
    }
    return entt::null;
  }

  PresentationSystem();
};
