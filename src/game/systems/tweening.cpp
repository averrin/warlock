#include <fmt/core.h>
#include <fmt/ranges.h>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <game/systems/tweening.hpp>
#include <liblog/liblog.hpp>
#include <ranges> // For ranges
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <game/systems/code_execution.hpp>

void TweeningSystem::fixedUpdate() {
  auto &current_state = entt::locator<State>::value();
  const std::chrono::duration<double, std::milli> delta(targetInterval);

  for (auto &f : current_state.registry.view<Frame>()) {
    auto frame = current_state.registry.get<Frame>(f);
    for (auto &c : frame.components) {
      if (c->state != ComponentState::DEACTIVATED) {
        for (auto [key, attribute] : c->data.attributes) {
          attribute->update(delta);
        }
      }
      if (c->time_switch >= 0) {
        c->time_switch -= delta.count();
        if (c->time_switch <= 0) {
          c->time_switch = -1;
          c->state = c->next_state;
          auto &emitter = entt::locator<event_emitter>::value();
          if (c->state == ComponentState::DEACTIVATED) {
            if (c->data.get<std::string>("type") == "Core") {
              emitter.publish(exec_lua_function{c, "stop"});
            }
            for (auto [key, attribute] : c->data.attributes) {
              attribute->resetEasing();
            }
          } else if (c->state == ComponentState::ACTIVE) {
            for (auto [key, attribute] : c->data.attributes) {
              attribute->resetEasing();
            }
            if (c->data.get<std::string>("type") == "Core") {
              emitter.publish(exec_lua_function{c, "start"});
            }
          }
        }
      }
    }
  }
}
