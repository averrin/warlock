#pragma once
#include <game/components/frame.hpp>
#include <game/system.hpp>
#include <tweeny.h>
#include <vector>

struct NetworkInfo {
  Metadata data;

  std::vector<int> frames;

  float production = 0;
  float consumption = 0;
  float accumulated = 0;
  float accumulated_available = 0;
  float battery_count = 0;
};

struct PowerInfo {
  std::vector<NetworkInfo> networks;
};

class PowerSystem : public System {
  int lastUpdate = 0;

public:
  std::map<int, std::shared_ptr<tweeny::tween<float>>> tweens;
  void update(std::chrono::duration<double, std::milli> delta) override;
};
