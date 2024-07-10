#pragma once
#include <app/editor.hpp>
#include <string>

// class MetaDataEditor : public Editor {
class MetaDataEditor {
public:
  std::string name;
  // void render() override;
  void render();
  std::map<std::string, std::string> cache = {};
  std::map<std::string, float> cache_f = {};

  // using Editor::Editor;
  MetaDataEditor(std::string name) : name(name) {}
};
