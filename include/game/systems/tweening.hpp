#pragma once
#include <deque>
#include <game/components/frame.hpp>
#include <game/system.hpp>
#include <vector>

class TweeningSystem : public System {
public:
  void fixedUpdate() override;
  TweeningSystem() : System(50, "Tweening") {}
};
