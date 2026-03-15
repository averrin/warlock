#pragma once
#include <chrono>
#include <liblog/liblog.hpp>
#include <game/well_known_entities.hpp>
#include <utils/entt.hpp>
#include <utils/jobs.hpp>
using hr_clock = std::chrono::high_resolution_clock;
#include <game/system.hpp>
#include <game/systems/code_execution.hpp>
#include <game/systems/input.hpp>
#include <game/systems/items.hpp>
#include <functional>
#include <mutex>
#include <queue>

class GameManager {
  LibLog::Logger log = LibLog::Logger(fmt::color::purple, "GM");
  std::chrono::time_point<hr_clock> lastUpdate;

public:
  GameManager();
  ~GameManager();

  std::vector<std::string> components = {};

  std::vector<std::shared_ptr<System>> systems;
  std::shared_ptr<CodeExecutionSystem> exec;
  std::shared_ptr<ItemsSystem> items;
  std::shared_ptr<InputSystem> input;

  void init(LibLog::Logger parentLog);
  void start();
  bool started = false;
  void serve();

  void loadData();
  void saveData();
  entt::entity addFrame(std::string name);
  int addFrameFromBlueprint(std::string name);
  entt::entity addConnection(int source, int target, ConnectionType);

  std::shared_ptr<Job> startJob;
  std::recursive_mutex updateMutex;

  void initScript(entt::registry &registry, entt::entity entity);
  void releaseScript(entt::registry &registry, entt::entity entity);

  void enqueueCommand(std::function<void()> cmd);

  void startFramePlacement();

private:
  std::queue<std::function<void()>> pending_commands_;
  std::mutex command_mutex_;
};
