#pragma once
#include <deque>
#include <game/components/frame.hpp>
#include <game/components/items.hpp>
#include <game/item_loader.hpp>
#include <game/system.hpp>
#include <iostream>
#include <random>
#include <utils/entt.hpp>
#include <vector>

class ItemsSystem : public System {
  bool ready = false;
  std::unordered_map<int, double> lastExecutionTime;
  std::unordered_map<int, double> conveyorExecutionTime;
  std::unordered_map<int, double> conveyor_progress_by_connection_;
  std::unordered_map<int, std::string> lastRecipe;
  std::unordered_map<std::string, std::shared_ptr<Modifier>> modifiers;

  void setConveyorProgressForConnection(int connDataId, int senderCompId, double timecost);

public:
  std::shared_ptr<ItemLoader> loader;
  void fixedUpdate() override;
  double conveyorTransferProgress(int connectionDataId) const;
  /** Same shared_ptr as used in simulation for recipe power modifiers (consumption). */
  std::shared_ptr<Modifier> sharedRecipeModifier(const std::string &recipeName);
  ItemsSystem();
};
