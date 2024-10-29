#pragma once
#include <game/components/items.hpp>
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class ItemLoader {
public:
  void load_items(const std::string &path, sol::state &lua);
  void load_recipes(const std::string &path, sol::state &lua);
  const std::unordered_map<std::string, ItemDefinition> &get_items() const;
  const std::vector<RecipeDefinition> &get_recipes() const;

private:
  std::unordered_map<std::string, ItemDefinition> item_map;
  std::vector<RecipeDefinition> recipes;

  ItemDefinition parse_item(const sol::table &t);
  RecipeDefinition parse_recipe(const sol::table &t);
};
