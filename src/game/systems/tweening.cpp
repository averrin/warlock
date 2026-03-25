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
  const float dt_sec = static_cast<float>(targetInterval) / 1000.f;

  for (auto e : current_state.registry.view<Frame, wl::transform>()) {
    auto &frame = current_state.registry.get<Frame>(e);
    if (!frame.move_tween_active) continue;
    auto &t = current_state.registry.get<wl::transform>(e);
    frame.move_tween_elapsed += dt_sec;
    float u = frame.move_tween_elapsed / frame.move_tween_duration;
    if (u >= 1.f) {
      t.position.x = frame.move_tween_ex;
      t.position.y = frame.move_tween_ey;
      frame.move_tween_active = false;
    } else {
      t.position.x = frame.move_tween_sx + (frame.move_tween_ex - frame.move_tween_sx) * u;
      t.position.y = frame.move_tween_sy + (frame.move_tween_ey - frame.move_tween_sy) * u;
    }
    current_state.registry.patch<wl::transform>(e);
  }

  for (auto &f : current_state.registry.view<Frame>()) {
    auto &frame = current_state.registry.get<Frame>(f);
    for (auto &c : frame.components) {
      if (!c) continue;
      if (c->state != ComponentState::DEACTIVATED) {
        for (auto [key, attribute] : c->data.attributes) {
          if (!attribute) continue;
          attribute->update(delta);
        }
      }
      if (c->time_switch >= 0) {
        c->time_switch -= delta.count();
        if (c->time_switch <= 0) {
          c->time_switch = -1;
          auto prev_state = c->state;
          c->state = c->next_state;
          auto &emitter = entt::locator<event_emitter>::value();
          if (c->state == ComponentState::DEACTIVATED) {
            emitter.publish(component_state_changed{
              frame.data.id, c->data.id, c->data.name,
              static_cast<int>(prev_state), static_cast<int>(c->state),
              "deactivation_complete"
            });
            if (c->data.get<std::string>("type") == "Core") {
              emitter.publish(exec_lua_function{c, "stop"});
            }
            for (auto [key, attribute] : c->data.attributes) {
              if (attribute) attribute->resetEasing();
            }
          } else if (c->state == ComponentState::ACTIVE) {
            emitter.publish(component_state_changed{
              frame.data.id, c->data.id, c->data.name,
              static_cast<int>(prev_state), static_cast<int>(c->state),
              "activation_complete"
            });
            for (auto [key, attribute] : c->data.attributes) {
              if (attribute) attribute->resetEasing();
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
