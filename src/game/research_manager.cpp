#include <game/research_manager.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

void ResearchManager::load(sol::state &lua, const fs::path &scripts_path) {
  nodes_.clear();
  locked_components_.clear();
  locked_recipes_.clear();
  unlocked_.clear();

  if (!fs::exists(scripts_path)) {
    return;
  }

  for (auto &entry : fs::directory_iterator(scripts_path)) {
    if (entry.path().extension() != ".lua") continue;
    std::ifstream f(entry.path());
    std::string src((std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
    sol::table spec = lua.load(src).call();
    if (!spec.valid()) continue;

    ResearchNode node;
    node.name = spec["name"].get_or<std::string>("");
    node.description = spec["description"].get_or<std::string>("");
    node.icon = spec["icon"].get_or<std::string>("");
    if (node.name.empty()) continue;

    sol::optional<sol::table> cost_t = spec["cost"];
    if (cost_t) {
      for (const auto &pair : cost_t.value()) {
        if (!pair.first.is<std::string>()) continue;
        std::string k = pair.first.as<std::string>();
        if (pair.second.is<int>()) node.cost[k] = pair.second.as<int>();
        else if (pair.second.is<double>()) node.cost[k] = static_cast<int>(pair.second.as<double>());
      }
    }

    auto parse_string_list = [](sol::optional<sol::table> t, std::vector<std::string> &out) {
      if (!t) return;
      for (const auto &pair : t.value()) {
        if (pair.second.is<std::string>()) out.push_back(pair.second.as<std::string>());
      }
    };

    parse_string_list(spec["requires"], node.requires);
    parse_string_list(spec["unlocks"], node.unlocks);
    parse_string_list(spec["unlocks_recipes"], node.unlocks_recipes);

    for (const auto &u : node.unlocks) locked_components_.insert(u);
    for (const auto &r : node.unlocks_recipes) locked_recipes_.insert(r);

    nodes_[node.name] = std::move(node);
  }

  // Auto-unlock nodes that are free and have no prerequisites
  for (const auto &[name, node] : nodes_) {
    if (node.cost.empty() && node.requires.empty()) {
      unlocked_.insert(name);
    }
  }
}

void ResearchManager::loadUnlocked(const fs::path &save_dir) {
  auto path = save_dir / "research.json";
  if (!fs::exists(path)) return;
  std::ifstream f(path);
  if (!f.is_open()) return;
  try {
    auto j = nlohmann::json::parse(f);
    if (j.contains("unlocked") && j["unlocked"].is_array()) {
      for (const auto &item : j["unlocked"]) {
        if (item.is_string()) unlocked_.insert(item.get<std::string>());
      }
    }
  } catch (...) {}
}

void ResearchManager::saveUnlocked(const fs::path &save_dir) const {
  auto path = save_dir / "research.json";
  nlohmann::json j;
  j["unlocked"] = nlohmann::json::array();
  for (const auto &name : unlocked_) {
    j["unlocked"].push_back(name);
  }
  std::ofstream f(path);
  f << j.dump(2);
}

bool ResearchManager::isUnlocked(const std::string &name) const {
  return unlocked_.count(name) > 0;
}

bool ResearchManager::prerequisitesMet(const std::string &name) const {
  auto it = nodes_.find(name);
  if (it == nodes_.end()) return false;
  for (const auto &req : it->second.requires) {
    if (!isUnlocked(req)) return false;
  }
  return true;
}

bool ResearchManager::isComponentLocked(const std::string &component_name) const {
  if (locked_components_.count(component_name) == 0) return false;
  for (const auto &uname : unlocked_) {
    auto it = nodes_.find(uname);
    if (it == nodes_.end()) continue;
    for (const auto &u : it->second.unlocks) {
      if (u == component_name) return false;
    }
  }
  return true;
}

bool ResearchManager::isRecipeLocked(const std::string &recipe_name) const {
  if (locked_recipes_.count(recipe_name) == 0) return false;
  for (const auto &uname : unlocked_) {
    auto it = nodes_.find(uname);
    if (it == nodes_.end()) continue;
    for (const auto &r : it->second.unlocks_recipes) {
      if (r == recipe_name) return false;
    }
  }
  return true;
}

void ResearchManager::unlock(const std::string &name) {
  if (nodes_.count(name)) unlocked_.insert(name);
}

void ResearchManager::unlockAll() {
  for (const auto &[name, node] : nodes_) {
    unlocked_.insert(name);
  }
}
