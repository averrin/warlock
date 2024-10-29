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
  std::unordered_map<int, std::string> lastRecipe;
  std::unordered_map<std::string, std::shared_ptr<Modifier>> modifiers;

public:
  std::shared_ptr<ItemLoader> loader;
  void fixedUpdate() override;
  ItemsSystem() : System(50, "Items") {
    loader = std::make_shared<ItemLoader>();
    fs::path PATH = entt::monostate<"path"_hs>{};
    auto &lua = entt::locator<sol::state>::value();
    loader->load_items((PATH / "scripts" / "items").string(), lua);
    loader->load_recipes((PATH / "scripts" / "recipes").string(), lua);

    const auto &items = loader->get_items();
    const auto &recipes = loader->get_recipes();

    // Print loaded items
    std::cout << "Loaded Items:" << std::endl;
    for (const auto &[name, item] : items) {
      std::cout << "Name: " << item.name
                << ", Description: " << item.description
                << ", Stack: " << item.stack << std::endl;
    }

    // Print loaded recipes
    std::cout << "Loaded Recipes:" << std::endl;
    for (const auto &recipe : recipes) {
      std::cout << "Name: " << recipe.name
                << ", Description: " << recipe.description << std::endl;
      std::cout << "Time Cost: " << recipe.timeCost
                << ", Power Cost: " << recipe.powerCost << std::endl;

      std::cout << "Inputs:" << std::endl;
      for (const auto &input : recipe.inputs) {
        std::cout << "  Item: " << input.item.name
                  << ", Amount: " << input.amount << std::endl;
      }

      std::cout << "Outputs:" << std::endl;
      for (const auto &output : recipe.outputs) {
        std::cout << "  Item: " << output.item.name
                  << ", Amount: " << output.amount << std::endl;
      }
    }
  }
};
