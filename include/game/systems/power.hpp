#pragma once
#include <deque>
#include <game/components/frame.hpp>
#include <game/system.hpp>
#include <utils/entt.hpp>
#include <vector>

/// True if frame_id is not on a power net, or steady load + extra fits production + battery headroom.
bool frame_network_can_afford_extra_consumption(entt::registry& registry, int frame_id, float extra);

struct NetworkInfo {
  Metadata data;

  std::vector<int> frames;

  float production = 0;
  float consumption = 0;
  float accumulated = 0;
  float accumulated_available = 0;
  int battery_count = 0;
  std::map<std::string, std::deque<float>> history;
};

struct PowerInfo {
  std::vector<NetworkInfo> networks;
};

class PowerSystem : public System {
  std::map<std::string, std::map<std::string, std::deque<float>>> history;

public:
  void fixedUpdate() override;

  PowerSystem() : System(50, "Power") {}
};
