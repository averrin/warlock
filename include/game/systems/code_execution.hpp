#pragma once
#include <deque>
#include <filesystem>
#include <fstream>
#include <game/components/frame.hpp>
#include <game/system.hpp>
#include <map>
#include <sstream>
#include <string>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <vector>
namespace fs = std::filesystem;

class CodeExecutionSystem : public System {
  std::map<int, sol::state> states = {};
  std::map<int, sol::table> compiled_scripts_ = {};

public:
  std::map<std::string, std::string> sources = {};
  std::map<std::string, std::string> blueprints = {};
  void fixedUpdate() override;
  CodeExecutionSystem() : System(50, "CodeExecution") {
    // read scripts/components folder, load all scripts
    fs::path PATH = entt::monostate<"path"_hs>();
    auto &lua = entt::locator<sol::state>::value();
    register_bindings(lua);
    auto path = PATH / fs::path("scripts/components");
    for (auto &entry : fs::directory_iterator(path)) {
      auto file = entry.path().string();
      auto name = entry.path().filename().string();
      std::ifstream t(file);
      std::string str((std::istreambuf_iterator<char>(t)),
                      std::istreambuf_iterator<char>());
      // sources[name] = str;
      // fmt::print("Script read: {}\n", name);
      sol::table spec = lua.load(str).call();
      auto title = spec["name"].get_or<std::string>("");
      sources[title] = str;
    }
    path = PATH / fs::path("scripts/blueprints");
    for (auto &entry : fs::directory_iterator(path)) {
      auto file = entry.path().string();
      auto name = entry.path().filename().string();
      std::ifstream t(file);
      std::string str((std::istreambuf_iterator<char>(t)),
                      std::istreambuf_iterator<char>());
      sol::table spec = lua.load(str).call();
      auto title = spec["name"].get_or<std::string>("");
      // fmt::print("Blueprint read: {} -> {}\n", name, title);
      blueprints[title] = str;
    }
    fmt::print("Blueprints: {}\n", blueprints.size());
  }

  sol::state &getState(int id);
  std::string getScript(std::string name);
  Frame *findFrameById(int id);

  void executeCoreFunction(std::shared_ptr<Component> component,
                           std::string function_name);

  void invalidateScript(int component_id);
  void cleanupFrame(int frame_id);
};
