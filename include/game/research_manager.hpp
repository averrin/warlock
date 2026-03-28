#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <sol/sol.hpp>
#include <string>
#include <vector>
namespace fs = std::filesystem;

class ResearchManager {
public:
  struct ResearchNode {
    std::string name;
    std::string description;
    std::string icon;
    std::map<std::string, int> cost;
    std::vector<std::string> requires;
    std::vector<std::string> unlocks;
    std::vector<std::string> unlocks_recipes;
  };

  void load(sol::state &lua, const fs::path &scripts_path);
  void loadUnlocked(const fs::path &save_dir);
  void saveUnlocked(const fs::path &save_dir) const;

  bool isUnlocked(const std::string &name) const;
  bool prerequisitesMet(const std::string &name) const;
  bool isComponentLocked(const std::string &component_name) const;
  bool isRecipeLocked(const std::string &recipe_name) const;
  const std::map<std::string, ResearchNode> &nodes() const { return nodes_; }
  const std::set<std::string> &unlocked() const { return unlocked_; }

  void unlock(const std::string &name);
  void unlockAll();

private:
  std::map<std::string, ResearchNode> nodes_;
  std::set<std::string> unlocked_;
  std::set<std::string> locked_components_;
  std::set<std::string> locked_recipes_;
};
