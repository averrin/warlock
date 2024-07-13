#pragma once
#include <app/editor.hpp>
#include <string>

class TilesetEditor {
public:
  std::string name;
  void render();
  std::map<std::string, std::string> cache = {};
  std::map<std::string, float> cache_f = {};

  TilesetEditor(std::string name) : name(name) {}
};
