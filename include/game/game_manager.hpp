#pragma once
#include <chrono>
#include <cstdint>
#include <liblog/liblog.hpp>
#include <map>
#include <string>
#include <game/well_known_entities.hpp>
#include <utils/entt.hpp>
#include <utils/jobs.hpp>
using hr_clock = std::chrono::high_resolution_clock;
#include <game/system.hpp>
#include <game/systems/code_execution.hpp>
#include <game/systems/items.hpp>
#include <game/research_manager.hpp>
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
  std::shared_ptr<ResearchManager> research;

  void init(LibLog::Logger &parentLog);
  void start();
  bool started = false;
  bool paused_ = false;
  uint64_t tick_count_ = 0;
  float speed_multiplier_ = 1.0f;
  void setPaused(bool p) { paused_ = p; }
  bool paused() const { return paused_; }
  void setSpeedMultiplier(float m) { if (m >= 1.0f && m <= 10.0f) speed_multiplier_ = m; }
  float speedMultiplier() const { return speed_multiplier_; }
  uint64_t tick_count() const { return tick_count_; }
  void serve();

  void loadData();
  void saveData();

  /** Global spendable currency (persisted in the state save file). */
  const std::map<std::string, int64_t> &spendablePool() const { return spendable_pool_; }
  void addSpendable(const std::string &name, int64_t delta);
  bool tryConsumeSpendable(const std::map<std::string, int> &cost, std::string &err);
  entt::entity addFrame(std::string name);
  int addFrameFromBlueprint(std::string name);
  entt::entity addConnection(int source, int target, ConnectionType);
  entt::entity addConnection(int source, int target, ConnectionType, ConnectionMedium medium);

  std::shared_ptr<Job> startJob;
  std::recursive_mutex updateMutex;

  void initScript(entt::registry &registry, entt::entity entity);
  void releaseScript(entt::registry &registry, entt::entity entity);

  void enqueueCommand(std::function<void()> cmd);

  // Autosave
  bool autosave_enabled_ = false;
  std::chrono::seconds autosave_interval_{60};
  std::chrono::time_point<hr_clock> last_save_{};
  std::string last_saved_at_;
  const std::string& lastSavedAt() const { return last_saved_at_; }

private:
  std::map<std::string, int64_t> spendable_pool_;
  void ensureSpendableKeysFromItems();
  void emitSpendablePool();

  std::queue<std::function<void()>> pending_commands_;
  std::mutex command_mutex_;
  std::chrono::time_point<hr_clock> last_state_push_{};
  std::chrono::time_point<hr_clock> last_env_push_{};
  std::chrono::time_point<hr_clock> last_power_push_{};
};
