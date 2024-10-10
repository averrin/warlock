#pragma once
#include <app/editor.hpp>
#include <string>
#include <vector>
#include <map>
#include <deque>

class GameEditor: public Editor {

public:
  std::string name;
  void render() override;
  std::map<std::string, std::string> cache = {};
  std::map<std::string, float> cache_f = {};
  std::map<std::string, int> cache_i = {};


  int history = 100;
  float x[100];
  GameEditor(std::string name) : Editor(name) {
    for (int i = 0; i <= history; ++i) {
      x[i] = i;
    }
  }
};

