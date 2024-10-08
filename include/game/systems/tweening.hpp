#pragma once
#include <game/components/frame.hpp>
#include <game/system.hpp>
#include <vector>
#include <deque>

class TweeningSystem : public System {
public:
  void fixedUpdate() override;
  TweeningSystem() : System(50) {}
};
