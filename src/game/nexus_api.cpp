#include <game/nexus_api.hpp>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <game/state.hpp>
#include <game/well_known_entities.hpp>

static std::vector<nexus_marker_event> internal_markers;

void NexusApi::showToast(std::string message, std::string type) {
  auto& emitter = entt::locator<event_emitter>::value();
  emitter.publish(nexus_toast_event{message, type});
}

void NexusApi::setMapMarker(float x, float y, std::string label, std::string color) {
  auto& emitter = entt::locator<event_emitter>::value();
  // Avoid duplicate labels in our internal copy
  std::erase_if(internal_markers, [&label](const auto& m) { return m.label == label; });
  internal_markers.push_back({x, y, label, color});
  emitter.publish(nexus_marker_event{x, y, label, color});
}

void NexusApi::clearMapMarkers() {
  auto& emitter = entt::locator<event_emitter>::value();
  internal_markers.clear();
  emitter.publish(nexus_clear_markers_event{});
}

void NexusApi::removeMapMarker(std::string label) {
  auto& emitter = entt::locator<event_emitter>::value();
  std::erase_if(internal_markers, [&label](const auto& m) { return m.label == label; });
  emitter.publish(nexus_remove_marker_event{label});
}

void NexusApi::setGlobalIndicator(std::string key, std::string label, std::string value, std::string color) {
  auto& emitter = entt::locator<event_emitter>::value();
  emitter.publish(nexus_indicator_event{key, label, value, color});
}

sol::table NexusApi::getMapMarkers(sol::this_state s) {
  sol::state_view lua(s);
  sol::table tbl = lua.create_table();
  int i = 1;
  for (const auto& marker : internal_markers) {
    sol::table t = lua.create_table();
    t["x"] = marker.x;
    t["y"] = marker.y;
    t["label"] = marker.label;
    t["color"] = marker.color;
    tbl[i++] = t;
  }
  return tbl;
}

float NexusApi::getMouseX() {
  auto& state = entt::locator<State>::value();
  auto& wk = entt::locator<WellKnownEntities>::value();
  if (wk.environment != entt::null && state.registry.valid(wk.environment) && state.registry.all_of<Environment>(wk.environment)) {
    auto& env = state.registry.get<Environment>(wk.environment);
    return env.mouseX;
  }
  return 0.0f;
}

float NexusApi::getMouseY() {
  auto& state = entt::locator<State>::value();
  auto& wk = entt::locator<WellKnownEntities>::value();
  if (wk.environment != entt::null && state.registry.valid(wk.environment) && state.registry.all_of<Environment>(wk.environment)) {
    auto& env = state.registry.get<Environment>(wk.environment);
    return env.mouseY;
  }
  return 0.0f;
}
