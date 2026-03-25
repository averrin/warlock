#include <fmt/core.h>
#include <fmt/ranges.h>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <game/data_link.hpp>
#include <game/systems/code_execution.hpp>
#include <game/well_known_entities.hpp>
#include <liblog/liblog.hpp>
#include <ranges> // For ranges
#include <string>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>

void CodeExecutionSystem::emitLog(const std::string& source,
                                  const std::string& status,
                                  const nlohmann::json& args) const {
  if (log_callback_) {
    log_callback_(source, status, args);
  }
}

void CodeExecutionSystem::installPrintOverride(sol::state& lua, int frame_id) {
  // Capture `this` and frame_id so print() output is routed through the log callback.
  lua["print"] = [this, frame_id](sol::variadic_args va) {
    std::string message;
    for (size_t i = 0; i < va.size(); ++i) {
      if (i > 0) message += "\t";
      sol::object obj = va[i];
      if (obj.is<std::string>()) {
        message += obj.as<std::string>();
      } else if (obj.is<double>()) {
        message += fmt::format("{}", obj.as<double>());
      } else if (obj.is<bool>()) {
        message += obj.as<bool>() ? "true" : "false";
      } else if (obj.get_type() == sol::type::nil) {
        message += "nil";
      } else {
        message += sol::type_name(va.lua_state(), obj.get_type());
      }
    }
    fmt::print("[lua:{}] {}\n", frame_id, message);
    emitLog("lua.print", "ok", {{"frame_id", frame_id}, {"message", message}});
  };
}

sol::state &CodeExecutionSystem::getState(int id) {
  if (states.find(id) == states.end()) {
    // fmt::print("Creating new state for frame {}\n", id);
    states[id] = sol::state();
    states[id].open_libraries(sol::lib::base, sol::lib::package,
                              sol::lib::string, sol::lib::table, sol::lib::math,
                              sol::lib::os, sol::lib::io);
    register_bindings(states[id]);
    installPrintOverride(states[id], id);
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

void CodeExecutionSystem::invalidateScript(int component_id) {
  compiled_scripts_.erase(component_id);
}

void CodeExecutionSystem::invalidateAllScripts() {
  compiled_scripts_.clear();
}

void CodeExecutionSystem::cleanupFrame(int frame_id) {
  states.erase(frame_id);
}

void CodeExecutionSystem::executeCoreFunction(std::shared_ptr<Component> c,
                                              std::string function_name) {
  auto &current_state = entt::locator<State>::value();
  auto comp_id = c->data.id;
  auto prev_state = c->state;

  // frame_id is not persisted on Component; after save/load it stays -1 unless we repair.
  Frame *frame_ptr = findFrameById(c->frame_id);
  if (!frame_ptr) {
    for (auto e : current_state.registry.view<Frame>()) {
      auto &f = current_state.registry.get<Frame>(e);
      for (const auto &comp : f.components) {
        if (comp == c) {
          c->frame_id = f.data.id;
          frame_ptr = &f;
          break;
        }
      }
      if (frame_ptr) break;
    }
  }
  const int fid = c->frame_id;

  if (!frame_ptr) {
    c->state = ComponentState::COMP_ERROR;
    c->error = "frame not found for code execution";
    emitLog("lua.runtime", "error", {
        {"frame_id", fid},
        {"component_id", comp_id},
        {"function", function_name},
        {"error", c->error},
        {"prev_state", static_cast<int>(prev_state)},
        {"new_state", static_cast<int>(c->state)}});
    return;
  }

  // get Environment
  auto &wk = entt::locator<WellKnownEntities>::value();
  if (wk.environment != entt::null && current_state.registry.valid(wk.environment) &&
      current_state.registry.all_of<Environment>(wk.environment)) {
    auto &environment = current_state.registry.get<Environment>(wk.environment);
    getState(fid).set("environment", environment);
  }

  getState(fid).set("frame", frame_ptr);

  // Cache compiled script — only recompile when not cached
  if (compiled_scripts_.find(comp_id) == compiled_scripts_.end()) {
    if (!c->data.has("code")) {
      c->state = ComponentState::COMP_ERROR;
      c->error = "Core has no code attribute";
      emitLog("lua.compile", "error", {
          {"frame_id", fid},
          {"component_id", comp_id},
          {"error", c->error},
          {"prev_state", static_cast<int>(prev_state)},
          {"new_state", static_cast<int>(c->state)}});
      return;
    }
    auto code = c->data.get<std::string>("code");
    if (code.empty()) {
      c->state = ComponentState::COMP_ERROR;
      c->error = "empty code";
      emitLog("lua.compile", "error", {
        {"frame_id", fid}, {"component_id", comp_id},
        {"error", c->error},
        {"prev_state", static_cast<int>(prev_state)},
        {"new_state", static_cast<int>(c->state)}
      });
      return;
    }
    sol::safe_function_result result =
        getState(fid).safe_script(code, sol::script_pass_on_error);
    if (!result.valid()) {
      sol::error err = result;
      fmt::print("Lua script error: {}\n", err.what());
      c->state = ComponentState::COMP_ERROR;
      c->error = err.what();
      emitLog("lua.compile", "error", {
        {"frame_id", fid}, {"component_id", comp_id},
        {"error", c->error},
        {"prev_state", static_cast<int>(prev_state)},
        {"new_state", static_cast<int>(c->state)}
      });
      return;
    }
    sol::object result_obj = result;
    if (result_obj.get_type() != sol::type::table) {
      c->state = ComponentState::COMP_ERROR;
      c->error = "script must return a table, got " +
                 std::string(sol::type_name(getState(fid).lua_state(), result_obj.get_type()));
      emitLog("lua.compile", "error", {
        {"frame_id", fid}, {"component_id", comp_id},
        {"error", c->error},
        {"prev_state", static_cast<int>(prev_state)},
        {"new_state", static_cast<int>(c->state)}
      });
      return;
    }
    compiled_scripts_[comp_id] = result_obj.as<sol::table>();
    emitLog("lua.compile", "ok", {
      {"frame_id", fid}, {"component_id", comp_id}
    });
  }

  auto &script_table = compiled_scripts_[comp_id];
  sol::object fn_obj = script_table[function_name];
  if (fn_obj.valid() && fn_obj.is<sol::function>()) {
    sol::protected_function fn(fn_obj.as<sol::function>());
    sol::protected_function_result result = fn(frame_ptr);
    if (!result.valid()) {
      sol::error err = result;
      fmt::print("Lua runtime error: {}\n", err.what());
      c->state = ComponentState::COMP_ERROR;
      c->error = err.what();
      emitLog("lua.runtime", "error", {
        {"frame_id", fid}, {"component_id", comp_id},
        {"function", function_name},
        {"error", c->error},
        {"prev_state", static_cast<int>(prev_state)},
        {"new_state", static_cast<int>(c->state)}
      });
    } else {
      c->error.clear();
      if (c->state == ComponentState::COMP_ERROR) {
        c->state = ComponentState::ACTIVE;
      }
    }
  } else {
    c->state = ComponentState::COMP_ERROR;
    c->error = "function '" + function_name + "' not found in script table";
    emitLog("lua.runtime", "error", {
      {"frame_id", fid}, {"component_id", comp_id},
      {"function", function_name},
      {"error", c->error},
      {"prev_state", static_cast<int>(prev_state)},
      {"new_state", static_cast<int>(c->state)}
    });
  }
}

void CodeExecutionSystem::fixedUpdate() {
  recompute_data_link_counterparts();

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
