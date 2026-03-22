#pragma once
#include <game/system.hpp>

class WirelessConnectionSystem : public System {
public:
  void fixedUpdate() override;
  WirelessConnectionSystem() : System(1000, "WirelessConnection") {} // Run once a second to avoid overhead
};
