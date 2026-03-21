#include <algorithm>
#include <cctype>
#include <game/components/frame.hpp>
#include <game/lua_completion.hpp>
#include <game/nexus_api.hpp>
#include <game/oracle.hpp>
#include <game/systems/code_execution.hpp>
#include <game/systems/power.hpp>
#include <game/state.hpp>
#include <game/well_known_entities.hpp>
#include <sol/sol.hpp>
#include <vector>

namespace {

void appendSortedUnique(std::vector<std::string>& out) {
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
}

void appendCStringKeys(const char* const* keys, size_t n, std::vector<std::string>& out) {
  for (size_t i = 0; i < n; ++i) {
    out.push_back(keys[i]);
  }
}

void appendKeysForObject(sol::object obj, std::vector<std::string>& out) {
  if (!obj.valid() || obj.get_type() == sol::type::nil) {
    return;
  }
  sol::type t = obj.get_type();
  if (t == sol::type::table) {
    sol::table tbl = obj;
    for (const auto& pair : tbl) {
      if (pair.first.get_type() == sol::type::string) {
        out.push_back(pair.first.as<std::string>());
      } else if (pair.first.get_type() == sol::type::number) {
        double n = pair.first.as<double>();
        if (n == std::floor(n)) {
          out.push_back(std::to_string(static_cast<int>(n)));
        }
      }
    }
    return;
  }
  if (t == sol::type::lua_nil) {
    return;
  }

  if (obj.is<Frame*>()) {
    static const char* keys[] = {"data",             "components",       "getComponentByType", "getPowerInfo",
                                 "getStorages",      "hasComponentType", "getComponentsByType", "getComponentByName"};
    appendCStringKeys(keys, sizeof(keys) / sizeof(keys[0]), out);
    return;
  }
  if (obj.is<Environment*>()) {
    static const char* keys[] = {"temperature", "minutes"};
    appendCStringKeys(keys, sizeof(keys) / sizeof(keys[0]), out);
    return;
  }
  if (obj.is<Metadata*>()) {
    static const char* keys[] = {"id", "name", "description", "attributes"};
    appendCStringKeys(keys, sizeof(keys) / sizeof(keys[0]), out);
    return;
  }
  if (obj.is<std::shared_ptr<Component>>()) {
    static const char* keys[] = {
        "data",           "api",            "state",          "size",       "require",
        "conflict",       "activate",       "deactivate",     "storage",    "sendRaw",
        "send",           "injectRaw",      "injectPacket",   "readRaw",    "read",
        "getCounterpart", "queueDepthRaw",  "queueDepthPacket"};
    appendCStringKeys(keys, sizeof(keys) / sizeof(keys[0]), out);
    return;
  }
  if (obj.is<Attribute*>() || obj.is<std::shared_ptr<Attribute>>()) {
    static const char* keys[] = {"GetTitle",       "GetDescription", "GetType", "SetBaseValue",
                                 "GetBaseValue",   "GetFinalValue",   "SetEasing"};
    appendCStringKeys(keys, sizeof(keys) / sizeof(keys[0]), out);
    return;
  }
  if (obj.is<NetworkInfo>()) {
    static const char* keys[] = {"production", "consumption", "accumulated", "accumulated_available",
                                 "battery_count"};
    appendCStringKeys(keys, sizeof(keys) / sizeof(keys[0]), out);
    return;
  }
  if (obj.is<ItemDefinition*>()) {
    static const char* keys[] = {"name", "description", "stack"};
    appendCStringKeys(keys, sizeof(keys) / sizeof(keys[0]), out);
    return;
  }
  if (obj.is<ItemStack*>() || obj.is<std::shared_ptr<ItemStack>>()) {
    static const char* keys[] = {"item", "amount"};
    appendCStringKeys(keys, sizeof(keys) / sizeof(keys[0]), out);
    return;
  }
  if (obj.is<ItemStorage*>() || obj.is<std::shared_ptr<ItemStorage>>()) {
    static const char* keys[] = {"slots", "slotsCount", "take", "getStackByItem",
                                 "transferTo", "transferFrom"};
    appendCStringKeys(keys, sizeof(keys) / sizeof(keys[0]), out);
    return;
  }
  if (obj.is<RecipeDefinition*>()) {
    static const char* keys[] = {"name", "description", "outputs", "inputs",
                                 "timeCost", "powerCost"};
    appendCStringKeys(keys, sizeof(keys) / sizeof(keys[0]), out);
    return;
  }
  if (obj.is<Oracle*>()) {
    static const char* keys[] = {"getWirelessDataFrames", "getWiredFrames"};
    appendCStringKeys(keys, sizeof(keys) / sizeof(keys[0]), out);
    return;
  }
  if (obj.is<NexusApi*>()) {
    static const char* keys[] = {"showToast", "setMapMarker", "clearMapMarkers", "removeMapMarker",
                                 "getMapMarkers", "setGlobalIndicator", "getMouseX", "getMouseY"};
    appendCStringKeys(keys, sizeof(keys) / sizeof(keys[0]), out);
    return;
  }
}

bool pathCharsSafe(const std::string& path) {
  for (char c : path) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
      continue;
    }
    if (c == '.') {
      continue;
    }
    return false;
  }
  return true;
}

bool globalKeyShown(const std::string& key) {
  if (key.empty()) {
    return false;
  }
  if (key.size() >= 2 && key[0] == '_' && key[1] == '_') {
    return false;
  }
  return true;
}

void appendGlobalKeys(sol::state_view lua, std::vector<std::string>& out) {
  sol::table g = lua.globals();
  for (const auto& pair : g) {
    if (pair.first.get_type() != sol::type::string) {
      continue;
    }
    std::string k = pair.first.as<std::string>();
    if (globalKeyShown(k)) {
      out.push_back(std::move(k));
    }
  }
}

} // namespace

void syncFrameLuaEnvironment(CodeExecutionSystem& exec, int frame_id, Frame* frame_ptr) {
  auto& current_state = entt::locator<State>::value();
  auto& wk = entt::locator<WellKnownEntities>::value();
  sol::state& L = exec.getState(frame_id);
  if (wk.environment != entt::null && current_state.registry.valid(wk.environment) &&
      current_state.registry.all_of<Environment>(wk.environment)) {
    auto& environment = current_state.registry.get<Environment>(wk.environment);
    L.set("environment", environment);
  } else {
    L.set("environment", sol::nil);
  }
  if (frame_ptr) {
    L.set("frame", frame_ptr);
  } else {
    L.set("frame", sol::nil);
  }
}

std::vector<std::string> luaCompletionKeys(sol::state_view lua, const std::string& path) {
  std::vector<std::string> out;
  if (path.empty()) {
    appendGlobalKeys(lua, out);
    appendSortedUnique(out);
    return out;
  }
  if (!pathCharsSafe(path)) {
    return {};
  }

  const std::string chunk = "return " + path;
  sol::safe_function_result r = lua.safe_script(chunk, sol::script_pass_on_error);
  if (!r.valid()) {
    return {};
  }
  sol::object cur = r;
  appendKeysForObject(cur, out);
  appendSortedUnique(out);
  return out;
}
