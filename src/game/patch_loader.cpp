#include <game/patch_loader.hpp>
#include <filesystem>
#include <fmt/core.h>

namespace fs = std::filesystem;

void PatchLoader::load_patches(const std::string& path, sol::state& lua) {
  patch_types_.clear();
  
  if (!fs::exists(path)) {
    return;
  }
  
  for (const auto& entry : fs::directory_iterator(path)) {
    if (entry.path().extension() != ".lua") continue;
    
    std::string key = entry.path().stem().string();
    sol::table t = lua.load_file(entry.path().string()).call();
    
    PatchTypeDefinition def;
    def.key = key;
    def.name = t.get_or<std::string>("name", key);
    def.description = t.get_or<std::string>("description", "");
    def.item = t.get_or<std::string>("item", "");
    
    if (sol::table color = t["color"]; color.valid()) {
      def.color.r = color.get_or<uint8_t>("r", 255);
      def.color.g = color.get_or<uint8_t>("g", 200);
      def.color.b = color.get_or<uint8_t>("b", 50);
      def.color.a = color.get_or<uint8_t>("a", 100);
    }
    
    if (sol::table gen = t["generation"]; gen.valid()) {
      def.generation.min_width = gen.get_or(std::string("min_width"), 3);
      def.generation.max_width = gen.get_or(std::string("max_width"), 8);
      def.generation.min_height = gen.get_or(std::string("min_height"), 3);
      def.generation.max_height = gen.get_or(std::string("max_height"), 8);
      def.generation.fill_probability = gen.get_or(std::string("fill_probability"), 0.35f);
      def.generation.smoothing_rounds = gen.get_or(std::string("smoothing_rounds"), 5);
    }
    
    patch_types_[key] = def;
  }
}

const std::unordered_map<std::string, PatchTypeDefinition>& 
PatchLoader::get_patch_types() const {
  return patch_types_;
}

const PatchTypeDefinition* PatchLoader::get_patch_type(const std::string& key) const {
  auto it = patch_types_.find(key);
  return it != patch_types_.end() ? &it->second : nullptr;
}
