#pragma once
#include <game/components/frame.hpp>
#include <game/system.hpp>
#include <vector>
#include <deque>
#include <random>
#include <tweeny.h>
using tweeny::easing;

class EnvironmentSystem : public System {
  tweeny::tween<float> temp_tween;
  bool ready = false;
  std::random_device rd{};
  std::mt19937 gen{rd()};
public:
  void fixedUpdate() override;
  EnvironmentSystem() : System(50) {
  }
  void onNewDay(Environment&);

  float randomTemp(float min, float max){
    auto mean = (min + max) / 2;
    auto stddev = (max - min) / 6;
    std::normal_distribution d{mean, stddev};
    return d(gen);
  };
};
