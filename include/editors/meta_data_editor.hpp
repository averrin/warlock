#pragma once
#include <app/editor.hpp>

class MetaDataEditor : public Editor {
public:
  void render() override;
  std::map<std::string, std::string> cache = {};
  std::map<std::string, float> cache_f = {};

  using Editor::Editor;
};
