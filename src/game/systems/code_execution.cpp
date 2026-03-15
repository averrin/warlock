#include <fmt/core.h>
#include <fmt/ranges.h>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <game/systems/code_execution.hpp>
#include <liblog/liblog.hpp>
#include <ranges> // For ranges
#include <string>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>

sol::state &CodeExecutionSystem::getState(int id) {
  if (states.find(id) == states.end()) {
    fmt::print("Creating new state for frame {}\n", id);
    states[id] = sol::state();
    states[id].open_libraries(sol::lib::base, sol::lib::package,
                              sol::lib::string, sol::lib::table, sol::lib::math,
                              sol::lib::os, sol::lib::io);
    register_bindings(states[id]);
  }
  return states[id];
}

std::string CodeExecutionSystem::getScript(std::string name) {
  if (sources.find(name) == sources.end()) {
    fmt::print("Script not found: {}\n", name);
    throw std::runtime_error("Script not found");
  }
  return sources[name];
}

Frame *CodeExecutionSystem::findFrameById(int id) {
  auto &current_state = entt::locator<State>::value();
  for (auto &e : current_state.registry.view<Frame>()) {
    auto &frame = current_state.registry.get<Frame>(e);
    if (frame.data.id == id) {
      return &frame;
    }
  }
  return nullptr;
}

void CodeExecutionSystem::executeCoreFunction(std::shared_ptr<Component> c,
                                              std::string function_name) {
  auto &current_state = entt::locator<State>::value();
  auto fid = c->frame_id;

  // get Environment
  for (auto &env : current_state.registry.view<Environment>()) {
    auto &environment = current_state.registry.get<Environment>(env);
    getState(fid).set("environment", environment);
    break;
  }

  auto *frame_ptr = findFrameById(fid);
  if (!frame_ptr) return;

  auto code = c->data.get<std::string>("code");
  getState(fid).set("frame", frame_ptr);
  sol::safe_function_result result =
      getState(fid).safe_script(code, sol::script_pass_on_error);
  if (!result.valid()) {
    sol::error err = result;
    fmt::print("Lua script error: {}\n", err.what());
    c->state = ComponentState::COMP_ERROR;
    c->error = err.what();
  } else {
    auto f = result.get<sol::table>()[function_name];
    if (f.valid()) {
      sol::safe_function_result result = f(frame_ptr);
      if (!result.valid()) {
        sol::error err = result;
        fmt::print("Lua script error: {}\n", err.what());
        c->state = ComponentState::COMP_ERROR;
        c->error = err.what();
      }
    } else {
      c->state = ComponentState::COMP_ERROR;
      c->error = "";
    }
  }
}

void CodeExecutionSystem::fixedUpdate() {
  auto &current_state = entt::locator<State>::value();

  for (auto &f : current_state.registry.view<Frame>()) {
    auto &frame = current_state.registry.get<Frame>(f);
    for (auto &c : frame.components) {
      if (c->state != ComponentState::ACTIVE) {
        continue;
      }
      if (c->data.get<std::string>("type") == "Core") {
        executeCoreFunction(c, "update");
      }
    }
  }
}
