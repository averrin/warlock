#pragma once
#include <app/editor.hpp>
#include <string>
#include <vector>

class GameEditor {

public:
  std::string name;
  void render();
  std::map<std::string, std::string> cache = {};
  std::map<std::string, float> cache_f = {};
  std::map<std::string, int> cache_i = {};

  GameEditor(std::string name) : name(name) {
  }
};

