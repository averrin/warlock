#pragma once
#include <map>
#include <string>

class Editor {
public:
  virtual void render() = 0;
  std::string name;

  Editor(std::string name) : name(name) {}
};

