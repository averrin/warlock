#pragma once
#include <app/editor.hpp>
#include <string>
#include <vector>

class PowerEditor {
  std::vector<float> history = {};

public:
  std::string name;
  void render();
  std::map<std::string, std::string> cache = {};
  std::map<std::string, float> cache_f = {};

  PowerEditor(std::string name) : name(name) {}
};
