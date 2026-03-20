#pragma once

#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

struct PatchColor {
  uint8_t r = 255, g = 200, b = 50, a = 100;
};

struct PatchGeneration {
  int min_width = 3, max_width = 8;
  int min_height = 3, max_height = 8;
  float fill_probability = 0.35f;
  int smoothing_rounds = 5;
};

struct PatchTypeDefinition {
  std::string key;
  std::string name;
  std::string description;
  std::string item;
  bool obstacle = false;
  PatchColor color;
  PatchGeneration generation;
};

class PatchLoader {
public:
  void load_patches(const std::string& path, sol::state& lua);
  const std::unordered_map<std::string, PatchTypeDefinition>& get_patch_types() const;
  const PatchTypeDefinition* get_patch_type(const std::string& key) const;
  
private:
  std::unordered_map<std::string, PatchTypeDefinition> patch_types_;
};
