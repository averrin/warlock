#pragma once
#include <chrono>

class System {
public:
  virtual void update(std::chrono::duration<double, std::milli> delta) = 0;
};
