#pragma once
#include <app/editor.hpp>
#include <editors/entity_tree_editor.hpp>
#include <game/state.hpp>
#include <liblog/liblog.hpp>
#include <string>
#include <utils/jobs.hpp>
#include <vector>

class StateEditor {
  LibLog::Logger log = LibLog::Logger(fmt::color::pink, "STED");
  std::shared_ptr<EntityTreeEditor> et_editor = nullptr;

public:
  std::shared_ptr<Job> startJob = nullptr;
  std::vector<std::shared_ptr<State>> states = {};
  std::string name = "";
  bool started = true;
  void start();
  void render();
  std::map<std::string, std::string> cache = {};
  std::map<std::string, float> cache_f = {};

  // using Editor::Editor;
  StateEditor(std::string name);
};
