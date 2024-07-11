#pragma once
#include <liblog/liblog.hpp>
#include <utils/jobs.hpp>
#include <utils/entt.hpp>
#include <chrono>
using hr_clock = std::chrono::high_resolution_clock;

class GameManager {
  LibLog::Logger log = LibLog::Logger(fmt::color::purple, "GM");
  std::chrono::time_point<hr_clock> lastUpdate;

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

