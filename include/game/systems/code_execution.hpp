#pragma once
#include <game/components/frame.hpp>
#include <game/system.hpp>
#include <vector>
#include <deque>
#include <string>
#include <map>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>

class CodeExecutionSystem : public System {
  std::map<int, sol::state> states = {};
public:
  std::map<std::string, std::string> sources = {};
  void fixedUpdate() override;
  CodeExecutionSystem() : System(50) {
    //read scripts/components folder, load all scripts
    fs::path PATH = entt::monostate<"path"_hs>();
    auto path = PATH / fs::path("scripts/components");
    for (auto &entry : fs::directory_iterator(path)) {
      auto file = entry.path().string();
      auto name = entry.path().filename().string();
      std::ifstream t(file);
      std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
      sources[name] = str;
      fmt::print("Loaded script: {}\n", name);
    }
  }

  sol::state &getState(int id);
  std::string getScript(std::string name);

  void executeCoreFunction(std::shared_ptr<Component> component, std::string function_name);

};

