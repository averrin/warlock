#pragma once
#include <chrono>
#include <fmt/core.h>
#include <fmt/format.h>

class System {
protected:
    double targetInterval;  // Interval in milliseconds (e.g., 16.67ms for 60 FPS)
    double accumulatedTime; // Accumulated time

public:
  virtual void fixedUpdate() = 0;

  void update(std::chrono::duration<double, std::milli> deltaTime) {
        accumulatedTime += deltaTime.count();
    // fmt::print("Accumulated time: {}\n", accumulatedTime);
    // fmt::print("Current delta time: {}\n", deltaTime.count());
        while (accumulatedTime >= targetInterval) {
            fixedUpdate();    // Run the code at a fixed rate
            accumulatedTime -= targetInterval;  // Subtract the interval
        }
    }

  System(double targetInterval) : targetInterval(targetInterval), accumulatedTime(0) {
  }
};
