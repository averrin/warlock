#include <game/objective_manager.hpp>
#include <game/game_manager.hpp>
#include <game/nexus_api.hpp>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <utils/entt.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

void ObjectiveManager::load(sol::state &lua, const fs::path &scripts_path) {
  objectives_.clear();

  if (!fs::exists(scripts_path)) {
    return;
  }

  for (auto &entry : fs::directory_iterator(scripts_path)) {
    if (entry.path().extension() != ".lua") continue;
    std::ifstream f(entry.path());
    std::string src((std::istreambuf_iterator<char>(f)),
                    std::istreambuf_iterator<char>());
    auto result = lua.load(src);
    if (!result.valid()) continue;
    sol::protected_function_result call_result = result.call();
    if (!call_result.valid()) continue;
    sol::table spec = call_result;
    if (!spec.valid()) continue;

    Objective obj;
    obj.id = entry.path().stem().string();
    obj.name = spec["name"].get_or<std::string>("");
    if (obj.name.empty()) continue;

    obj.description = spec["description"].get_or<std::string>("");
    obj.icon = spec["icon"].get_or<std::string>("");
    obj.category = spec["category"].get_or<std::string>("");
    obj.hidden = spec["hidden"].get_or(false);
    obj.repeatable = spec["repeatable"].get_or(false);

    // Parse prerequisites
    sol::optional<sol::table> prereqs_t = spec["prerequisites"];
    if (prereqs_t) {
      for (const auto &pair : prereqs_t.value()) {
        if (pair.second.is<std::string>())
          obj.prerequisites.push_back(pair.second.as<std::string>());
      }
    }

    // Parse conditions
    sol::optional<sol::table> conds_t = spec["conditions"];
    if (conds_t) {
      for (const auto &pair : conds_t.value()) {
        Condition cond;
        if (pair.second.is<sol::protected_function>()) {
          cond.type = "function";
          cond.lua_func = pair.second.as<sol::protected_function>();
        } else if (pair.second.is<sol::table>()) {
          sol::table ct = pair.second.as<sol::table>();
          cond.type = ct["type"].get_or<std::string>("");
          cond.name = ct["name"].get_or<std::string>("");
          cond.amount = ct["amount"].get_or(0);
        } else {
          continue;
        }
        obj.conditions.push_back(std::move(cond));
      }
    }

    // Parse rewards
    sol::optional<sol::table> rewards_t = spec["rewards"];
    if (rewards_t) {
      for (const auto &pair : rewards_t.value()) {
        if (!pair.second.is<sol::table>()) continue;
        sol::table rt = pair.second.as<sol::table>();
        Reward reward;
        reward.type = rt["type"].get_or<std::string>("");
        reward.name = rt["name"].get_or<std::string>("");
        reward.amount = rt["amount"].get_or(0);
        reward.message = rt["message"].get_or<std::string>("");
        obj.rewards.push_back(std::move(reward));
      }
    }

    objectives_[obj.id] = std::move(obj);
  }
}

void ObjectiveManager::loadState(const fs::path &save_dir) {
  auto path = save_dir / "objectives.json";
  if (!fs::exists(path)) return;
  std::ifstream f(path);
  if (!f.is_open()) return;
  try {
    auto j = nlohmann::json::parse(f);
    if (j.contains("completed") && j["completed"].is_array()) {
      for (const auto &item : j["completed"]) {
        if (item.is_string()) completed_.insert(item.get<std::string>());
      }
    }
    if (j.contains("counts") && j["counts"].is_object()) {
      for (auto it = j["counts"].begin(); it != j["counts"].end(); ++it) {
        if (it.value().is_number_integer()) {
          completion_counts_[it.key()] = it.value().get<int>();
        }
      }
    }
  } catch (...) {}
}

void ObjectiveManager::saveState(const fs::path &save_dir) const {
  auto path = save_dir / "objectives.json";
  nlohmann::json j;
  j["completed"] = nlohmann::json::array();
  for (const auto &id : completed_) {
    j["completed"].push_back(id);
  }
  j["counts"] = nlohmann::json::object();
  for (const auto &[id, count] : completion_counts_) {
    j["counts"][id] = count;
  }
  std::ofstream f(path);
  f << j.dump(2);
}

bool ObjectiveManager::isCompleted(const std::string &id) const {
  return completed_.count(id) > 0;
}

bool ObjectiveManager::prerequisitesMet(const std::string &id) const {
  auto it = objectives_.find(id);
  if (it == objectives_.end()) return false;
  for (const auto &prereq : it->second.prerequisites) {
    if (!isCompleted(prereq)) return false;
  }
  return true;
}

void ObjectiveManager::evaluate(GameManager &gm) {
  for (const auto &[id, obj] : objectives_) {
    if (isCompleted(id) && !obj.repeatable) continue;
    if (!prerequisitesMet(id)) continue;

    bool all_met = true;
    for (const auto &cond : obj.conditions) {
      if (!evaluateCondition(cond, gm)) {
        all_met = false;
        break;
      }
    }

    if (all_met) {
      complete(id, gm);
    }
  }
}

bool ObjectiveManager::evaluateCondition(const Condition &cond, GameManager &gm) {
  if (cond.type == "function") {
    if (!cond.lua_func.valid()) return false;
    auto result = cond.lua_func();
    if (!result.valid()) return false;
    return result.get<bool>();
  }
  if (cond.type == "research_unlocked") {
    return gm.research && gm.research->isUnlocked(cond.name);
  }
  if (cond.type == "spendable_gte") {
    auto pool = gm.spendablePool();
    auto it = pool.find(cond.name);
    return it != pool.end() && it->second >= cond.amount;
  }
  if (cond.type == "objective_complete") {
    return isCompleted(cond.name);
  }
  if (cond.type == "frame_count_gte") {
    if (!entt::locator<State>::has_value()) return false;
    auto &registry = entt::locator<State>::value().registry;
    auto view = registry.view<Frame>();
    return static_cast<int>(view.size()) >= cond.amount;
  }
  return false;
}

void ObjectiveManager::grantRewards(const Objective &obj, GameManager &gm) {
  auto &emitter = entt::locator<event_emitter>::value();
  for (const auto &reward : obj.rewards) {
    if (reward.type == "spendable") {
      gm.addSpendable(reward.name, reward.amount);
    } else if (reward.type == "research") {
      if (gm.research) {
        gm.research->unlock(reward.name);
      }
    } else if (reward.type == "toast") {
      emitter.publish(nexus_toast_event{reward.message, "success"});
    }
  }
}

void ObjectiveManager::complete(const std::string &id, GameManager &gm) {
  auto it = objectives_.find(id);
  if (it == objectives_.end()) return;

  if (it->second.repeatable) {
    // For repeatable objectives, remove from completed first so evaluate() picks it up again
    completed_.erase(id);
  }

  completed_.insert(id);
  completion_counts_[id]++;

  grantRewards(it->second, gm);

  auto &emitter = entt::locator<event_emitter>::value();
  emitter.publish(objective_completed_event{id, it->second.name});
}

void ObjectiveManager::reset(const std::string &id) {
  completed_.erase(id);
}
