#pragma once
#include <chrono>
#include <liblog/liblog.hpp>
#include <utils/entt.hpp>
#include <utils/jobs.hpp>
using hr_clock = std::chrono::high_resolution_clock;
#include <game/system.hpp>
#include <mutex>

class GameManager {
  LibLog::Logger log = LibLog::Logger(fmt::color::purple, "GM");
  std::chrono::time_point<hr_clock> lastUpdate;

  std::vector<std::shared_ptr<System>> systems;

public:
  GameManager();
  ~GameManager();

  std::vector<std::string> components = {};

  void init(LibLog::Logger parentLog);
  void start();
  bool started = false;
  void serve();

  void loadData();
  void saveData();
  entt::entity addFrame(std::string_view name);
  entt::entity addConnection(int source, int target);

  std::shared_ptr<Job> startJob;
  std::mutex updateMutex;

  void initScript(entt::registry &registry, entt::entity entity);
  void releaseScript(entt::registry &registry, entt::entity entity);
};
