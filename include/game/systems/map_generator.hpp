#pragma once

#include <game/mapgen_loader.hpp>
#include <nlohmann/json.hpp>
#include <entt/entt.hpp>
#include <random>
#include <cstdint>
#include <string>
#include <vector>

struct MapGenResult {
  uint64_t seed_used = 0;
  int patches_placed = 0;
  nlohmann::json summary;
  std::vector<std::string> biomes_used;
};

class MapGenerator {
public:
  MapGenResult generate(const MapGenConfig& config,
                        const std::vector<BiomeDef>& biomes,
                        const std::vector<MapGenFeatureDef>& features,
                        entt::registry& registry,
                        sol::state& lua);

private:
  std::mt19937 rng_;
  int map_w_ = 0;
  int map_h_ = 0;
  int center_x_ = 0;
  int center_y_ = 0;

  // Occupancy grid
  std::vector<std::vector<bool>> occupancy_;

  // Biome assignment grid (index into biomes vector, -1 = none)
  std::vector<std::vector<int>> biome_grid_;

  // Tracking placed patches for same-type spacing
  struct PlacedPatch {
    std::string type;
    int cx, cy; // centroid
  };
  std::vector<PlacedPatch> placed_;

  void initGrids(int w, int h);
  void generateBiomeLayout(const MapGenConfig& config,
                           const std::vector<BiomeDef>& biomes);
  void placeStarterPatches(const MapGenConfig& config,
                           entt::registry& registry,
                           nlohmann::json& summary);
  void placeObstacles(const MapGenConfig& config,
                      const std::vector<BiomeDef>& biomes,
                      entt::registry& registry,
                      nlohmann::json& summary);
  void placeResources(const MapGenConfig& config,
                      const std::vector<BiomeDef>& biomes,
                      entt::registry& registry,
                      nlohmann::json& summary);
  void executeFeatures(const std::vector<MapGenFeatureDef>& features,
                       const MapGenConfig& config,
                       entt::registry& registry,
                       sol::state& lua,
                       nlohmann::json& summary);

  entt::entity createPatch(entt::registry& registry,
                           const std::string& patch_type,
                           int origin_x, int origin_y,
                           std::vector<std::pair<int,int>> cells);

  float overlapFraction(const std::vector<std::pair<int,int>>& cells) const;
  void markOccupied(const std::vector<std::pair<int,int>>& cells);
  bool isInStarterZone(int x, int y, int radius) const;
  float distanceFromCenter(int x, int y) const;
  float minDistanceToSameType(const std::string& type, int cx, int cy) const;

  std::vector<std::pair<int,int>> generateBlobAtPosition(
      const std::string& patch_type, int ox, int oy);
};
