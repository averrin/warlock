#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <sol/sol.hpp>
#include <string>
#include <vector>
namespace fs = std::filesystem;

class GameManager;

class ObjectiveManager {
public:
  struct Condition {
    std::string type; // "function", "research_unlocked", "spendable_gte", "objective_complete", "frame_count_gte"
    std::string name;
    int amount = 0;
    sol::protected_function lua_func;
  };

  struct Reward {
    std::string type; // "spendable", "research", "toast"
    std::string name;
    int amount = 0;
    std::string message;
  };

  struct Objective {
    std::string id;
    std::string name;
    std::string description;
    std::string icon;
    std::string category;
    std::vector<std::string> prerequisites;
    std::vector<Condition> conditions;
    std::vector<Reward> rewards;
    bool hidden = false;
    bool repeatable = false;
  };

  void load(sol::state &lua, const fs::path &scripts_path);
  void loadState(const fs::path &save_dir);
  void saveState(const fs::path &save_dir) const;

  void evaluate(GameManager &gm);

  bool isCompleted(const std::string &id) const;
  bool prerequisitesMet(const std::string &id) const;
  const std::map<std::string, Objective> &objectives() const { return objectives_; }
  const std::set<std::string> &completed() const { return completed_; }

  void complete(const std::string &id, GameManager &gm);
  void reset(const std::string &id);

private:
  std::map<std::string, Objective> objectives_;
  std::set<std::string> completed_;
  std::map<std::string, int> completion_counts_;

  bool evaluateCondition(const Condition &cond, GameManager &gm);
  void grantRewards(const Objective &obj, GameManager &gm);
};
