#pragma once
#include <chrono>
#include <fmt/core.h>
#include <fmt/format.h>
#include <string>

class System {
protected:
  double accumulatedTime; // Accumulated time

public:
  float targetInterval;
  virtual void fixedUpdate() = 0;
  bool enabled = true;
  std::string name = "System";

  void update(std::chrono::duration<double, std::milli> deltaTime) {
    accumulatedTime += deltaTime.count();
    // fmt::print("Accumulated time: {}\n", accumulatedTime);
    // fmt::print("Current delta time: {}\n", deltaTime.count());
    while (accumulatedTime >= targetInterval) {
      fixedUpdate();                     // Run the code at a fixed rate
      accumulatedTime -= targetInterval; // Subtract the interval
    }
  }

  System(float targetInterval, std::string name)
      : targetInterval(targetInterval), accumulatedTime(0), name(name) {}
};
