#pragma once
#include <memory>
#include <string>
#include <vector>

struct ItemDefinition {
  std::string name;
  std::string description;
  int stack;
};

struct ItemStack {
  int id = -1;
  ItemDefinition item;
  int amount;

  ItemStack() : amount(0) {}
  ItemStack(ItemDefinition item, int amount) : item(item), amount(amount) {}
  ItemStack(const ItemStack &other)
      : item(other.item), amount(other.amount), id(other.id) {}
};

struct ItemSlot {
  int id = -1;
  std::shared_ptr<ItemStack> stack = nullptr;
};

class RecipeDefinition {
public:
  std::string name;
  std::string description;
  std::vector<ItemStack> outputs;
  std::vector<ItemStack> inputs;
  float timeCost;
  float powerCost;
  std::vector<std::string> availableOn;

  RecipeDefinition() : timeCost(0.0f), powerCost(0.0f) {}
};
