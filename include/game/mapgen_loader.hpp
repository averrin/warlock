#pragma once

#include <game/patch_loader.hpp>
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

struct BiomeDef {
  std::string key;
  std::string name;
  std::unordered_map<std::string, float> resource_weights;
  std::unordered_map<std::string, float> obstacle_weights;
  float obstacle_density = 1.0f;
  PatchColor tint;
};

struct MapGenFeatureDef {
  std::string key;
  std::string name;
  int stage = 5;
  sol::protected_function place;
};

struct MapGenResourceDef {
  std::string patch_type;
  int count_min = 2;
  int count_max = 5;
};

struct MapGenStarterPatch {
  std::string type;
  float distance = 12.0f;
};

struct MapGenConfig {
  int width = 200;
  int height = 200;
  uint64_t seed = 0;
  int starter_radius = 10;
  int biome_seed_count = 5;
  float max_overlap_fraction = 0.3f;
  int max_placement_attempts = 50;
  float min_same_type_distance = 20.0f;
  std::vector<MapGenStarterPatch> starter_patches;
  std::vector<MapGenResourceDef> base_resources;
  std::vector<MapGenResourceDef> base_obstacles;
};

class MapGenLoader {
public:
  void load(const std::string& mapgen_path, sol::state& lua);

  const MapGenConfig& config() const { return config_; }
  const std::vector<BiomeDef>& biomes() const { return biomes_; }
  const std::vector<MapGenFeatureDef>& features() const { return features_; }

private:
  MapGenConfig config_;
  std::vector<BiomeDef> biomes_;
  std::vector<MapGenFeatureDef> features_;

  void loadConfig(const std::string& path, sol::state& lua);
  void loadBiomes(const std::string& path, sol::state& lua);
  void loadFeatures(const std::string& path, sol::state& lua);
};
