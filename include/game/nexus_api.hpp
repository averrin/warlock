#pragma once
#include <string>
#include <utils/entt_lua.hpp>

struct nexus_toast_event {
  std::string message;
  std::string type;
};

struct nexus_marker_event {
  float x;
  float y;
  std::string label;
  std::string color;
};

struct nexus_clear_markers_event {
};

struct nexus_remove_marker_event {
  std::string label;
};

struct nexus_indicator_event {
  std::string key;
  std::string label;
  std::string value;
  std::string color;
};

class NexusApi {
public:
  void showToast(std::string message, std::string type);
  void setMapMarker(float x, float y, std::string label, std::string color);
  void clearMapMarkers();
  void removeMapMarker(std::string label);
  void setGlobalIndicator(std::string key, std::string label, std::string value, std::string color);

  float getMouseX();
  float getMouseY();
  sol::table getMapMarkers(sol::this_state s);
};
