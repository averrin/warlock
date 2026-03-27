#pragma once
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <game/components/frame.hpp>
#include <game/system.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <utility>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <vector>
namespace fs = std::filesystem;

// Callback signature for broadcasting log messages from the execution system.
// (source, status, args)
using ExecLogCallback = std::function<void(const std::string&, const std::string&, const nlohmann::json&)>;

class CodeExecutionSystem : public System {
  std::map<int, sol::state> states = {};
  std::map<int, sol::table> compiled_scripts_ = {};
  ExecLogCallback log_callback_;

  void emitLog(const std::string& source, const std::string& status,
               const nlohmann::json& args = nlohmann::json::object()) const;

  // Install a Lua print() override that routes output through log_callback_.
  void installPrintOverride(sol::state& lua, int frame_id);

public:
  std::map<std::string, std::string> sources = {};
  std::map<std::string, std::string> categories = {};
  std::map<std::string, std::string> blueprints = {};
  void fixedUpdate() override;

  // Set a callback that receives all execution log messages (print, errors, state changes).
  void setLogCallback(ExecLogCallback cb) { log_callback_ = std::move(cb); }

  CodeExecutionSystem() : System(50, "CodeExecution") {
    // read scripts/components folder, load all scripts
    fs::path PATH = entt::monostate<"path"_hs>();
    auto &lua = entt::locator<sol::state>::value();
    register_bindings(lua);
    auto path = PATH / fs::path("scripts/components");
    for (auto &entry : fs::directory_iterator(path)) {
      auto file = entry.path().string();
      auto name = entry.path().filename().string();
      std::ifstream t(file);
      std::string str((std::istreambuf_iterator<char>(t)),
                      std::istreambuf_iterator<char>());
      sol::table spec = lua.load(str).call();
      auto title = spec["name"].get_or<std::string>("");
      auto category = spec["category"].get_or<std::string>("Other");
      sources[title] = str;
      categories[title] = category;
    }
    path = PATH / fs::path("scripts/blueprints");
    for (auto &entry : fs::directory_iterator(path)) {
      auto file = entry.path().string();
      auto name = entry.path().filename().string();
      std::ifstream t(file);
      std::string str((std::istreambuf_iterator<char>(t)),
                      std::istreambuf_iterator<char>());
      sol::table spec = lua.load(str).call();
      auto title = spec["name"].get_or<std::string>("");
      blueprints[title] = str;
    }
    // fmt::print("Blueprints: {}\n", blueprints.size());
  }

  sol::state &getState(int id);
  std::string getScript(std::string name);
  Frame *findFrameById(int id);

  void executeCoreFunction(std::shared_ptr<Component> component,
                           std::string function_name);

  void invalidateScript(int component_id);
  /** Clear all compiled scripts (e.g. after state.load replaces registry). */
  void invalidateAllScripts();
  void cleanupFrame(int frame_id);

  /** Dispatch a component_state_changed event to Advanced Cores on the given frame.
   *  Calls onStateChange() in compiled script tables of cores that have a "memory" attribute. */
  void dispatchStateChange(int frame_id, int component_id,
                           const std::string& component_name,
                           int prev_state, int new_state,
                           const std::string& reason);
};

/** Rebind component `api` and attribute `inspector_meta` from Lua specs (not serialized); call after load and once exec exists. */
void refresh_component_apis(CodeExecutionSystem& exec);

/** Sync counterpart_id into the "counterpart" attribute on every data-connector component. Call after recompute_data_link_counterparts(). */
void sync_all_counterpart_attributes();

/** Convert a Lua table to nlohmann::json (arrays and objects auto-detected). */
nlohmann::json sol_table_to_json(sol::table t);

/** Convert nlohmann::json to a Lua object (inverse of sol_table_to_json). */
sol::object json_to_lua(sol::state_view L, const nlohmann::json& j);

/** Shared map: comp_id → { library_code, last_heartbeat_time }.
 *  Written by game_manager (reception), read by code_execution (injection). */
extern std::unordered_map<int, std::pair<std::string, std::chrono::steady_clock::time_point>> g_core_library;
