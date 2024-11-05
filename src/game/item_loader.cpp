#include <filesystem>
#include <fmt/format.h>
#include <game/item_loader.hpp>

namespace fs = std::filesystem;

ItemDefinition ItemLoader::parse_item(const sol::table &t) {
  ItemDefinition item;
  item.name = t["name"];
  item.description = t["description"];
  item.stack = t["stack"];
  return item;
}

RecipeDefinition ItemLoader::parse_recipe(const sol::table &t) {
  RecipeDefinition recipe;
  recipe.name = t["name"];
  recipe.description = t["description"];
  recipe.timeCost = t["timeCost"];
  recipe.powerCost = t["powerCost"];
  recipe.availableOn = t["available"].get_or<std::vector<std::string>>({});

  for (const auto &input : t["inputs"].get<std::vector<sol::table>>()) {
    ItemStack io;
    std::string item_name = input["name"];
    io.item = item_map.at(item_name); // Look up the item by name
    io.amount = input["amount"];
    recipe.inputs.push_back(io);
  }

  for (const auto &output : t["outputs"].get<std::vector<sol::table>>()) {
    ItemStack io;
    std::string item_name = output["name"];
    io.item = item_map.at(item_name); // Look up the item by name
    io.amount = output["amount"];
    recipe.outputs.push_back(io);
  }

  return recipe;
}

void ItemLoader::load_items(const std::string &path, sol::state &lua) {
  for (const auto &entry : fs::directory_iterator(path)) {
    // fmt::print("Loading item: {}\n", entry.path().string());
    sol::table t = lua.load_file(entry.path().string()).call();
    ItemDefinition item = parse_item(t);
    item_map[item.name] = item; // Store the item in the map
  }
}

void ItemLoader::load_recipes(const std::string &path, sol::state &lua) {
  for (const auto &entry : fs::directory_iterator(path)) {
    // fmt::print("Loading recipe: {}\n", entry.path().string());
    sol::table t = lua.load_file(entry.path().string()).call();
    recipes.push_back(parse_recipe(t));
  }
}

const std::unordered_map<std::string, ItemDefinition> &
ItemLoader::get_items() const {
  return item_map;
}

const std::vector<RecipeDefinition> &ItemLoader::get_recipes() const {
  return recipes;
}
