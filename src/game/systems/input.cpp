#include <fmt/core.h>
#include <fmt/ranges.h>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <game/systems/input.hpp>
#include <liblog/liblog.hpp>
#include <ranges> // For ranges
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>

void InputSystem::fixedUpdate() {
  auto &current_state = entt::locator<State>::value();
}
