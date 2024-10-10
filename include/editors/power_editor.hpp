#pragma once
#include <app/editor.hpp>
#include <string>
#include <vector>
#include <map>
#include <deque>

class PowerEditor: public Editor{

public:
  std::string name;
  void render() override;
  std::map<std::string, std::string> cache = {};
  std::map<std::string, float> cache_f = {};

  PowerEditor(std::string name) : Editor(name) {}
};
