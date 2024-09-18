#pragma once
#include <chrono>
#include <liblog/liblog.hpp>
#include <utils/entt.hpp>
#include <utils/jobs.hpp>
using hr_clock = std::chrono::high_resolution_clock;
#include <game/system.hpp>

class GameManager {
  LibLog::Logger log = LibLog::Logger(fmt::color::purple, "GM");
  std::chrono::time_point<hr_clock> lastUpdate;

  std::vector<std::shared_ptr<System>> systems;

public:
  GameManager();
  ~GameManager();

  void init(LibLog::Logger parentLog);
  void start();
  bool started = false;
  void serve();

  void loadData();
  void saveData();

  std::shared_ptr<Job> startJob;

  void initScript(entt::registry &registry, entt::entity entity);
  void releaseScript(entt::registry &registry, entt::entity entity);
};
