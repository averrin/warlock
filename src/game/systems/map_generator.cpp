#include <game/systems/map_generator.hpp>
#include <game/systems/patch_generation.hpp>
#include <game/components/resource_patch.hpp>
#include <game/patch_loader.hpp>
#include <game/well_known_entities.hpp>
#include <utils/entt.hpp>
#include <utils/entt_draw.hpp>
#include <utils/entt_tools.hpp>
#include <liblog/liblog.hpp>
#include <fmt/core.h>
#include <algorithm>
#include <cmath>

static LibLog::Logger mapgen_log = LibLog::Logger(fmt::color::green, "MAPGEN");

// ── Helpers ─────────────────────────────────────────────────────────────────

void MapGenerator::initGrids(int w, int h) {
  map_w_ = w;
  map_h_ = h;
  center_x_ = w / 2;
  center_y_ = h / 2;
  occupancy_.assign(h, std::vector<bool>(w, false));
  biome_grid_.assign(h, std::vector<int>(w, -1));
  placed_.clear();
}

float MapGenerator::distanceFromCenter(int x, int y) const {
  float dx = static_cast<float>(x - center_x_);
  float dy = static_cast<float>(y - center_y_);
  return std::sqrt(dx * dx + dy * dy);
}

bool MapGenerator::isInStarterZone(int x, int y, int radius) const {
  return distanceFromCenter(x, y) < static_cast<float>(radius);
}

float MapGenerator::overlapFraction(const std::vector<std::pair<int,int>>& cells) const {
  if (cells.empty()) return 0.0f;
  int overlap = 0;
  for (const auto& [x, y] : cells) {
    if (x >= 0 && x < map_w_ && y >= 0 && y < map_h_ && occupancy_[y][x]) {
      ++overlap;
    }
  }
  return static_cast<float>(overlap) / static_cast<float>(cells.size());
}

void MapGenerator::markOccupied(const std::vector<std::pair<int,int>>& cells) {
  for (const auto& [x, y] : cells) {
    if (x >= 0 && x < map_w_ && y >= 0 && y < map_h_) {
      occupancy_[y][x] = true;
    }
  }
}

float MapGenerator::minDistanceToSameType(const std::string& type, int cx, int cy) const {
  float min_dist = 1e9f;
  for (const auto& p : placed_) {
    if (p.type != type) continue;
    float dx = static_cast<float>(cx - p.cx);
    float dy = static_cast<float>(cy - p.cy);
    float d = std::sqrt(dx * dx + dy * dy);
    if (d < min_dist) min_dist = d;
  }
  return min_dist;
}

std::vector<std::pair<int,int>> MapGenerator::generateBlobAtPosition(
    const std::string& patch_type, int ox, int oy) {
  auto& loader = entt::locator<PatchLoader>::value();
  auto* def = loader.get_patch_type(patch_type);
  if (!def) return {};

  std::uniform_int_distribution<int> w_dis(def->generation.min_width, def->generation.max_width);
  std::uniform_int_distribution<int> h_dis(def->generation.min_height, def->generation.max_height);
  int w = w_dis(rng_);
  int h = h_dis(rng_);

  auto cells = generateBlob(w, h, def->generation.fill_probability,
                            def->generation.smoothing_rounds, rng_);
  // Offset to world position
  for (auto& [cx, cy] : cells) {
    cx += ox;
    cy += oy;
  }
  return cells;
}

// ── Entity creation ─────────────────────────────────────────────────────────

entt::entity MapGenerator::createPatch(entt::registry& registry,
                                       const std::string& patch_type,
                                       int origin_x, int origin_y,
                                       std::vector<std::pair<int,int>> cells) {
  auto& loader = entt::locator<PatchLoader>::value();
  auto* def = loader.get_patch_type(patch_type);
  if (!def) return entt::null;

  auto e = registry.create();

  hf::meta meta;
  meta.name = def->name;
  meta.id = fmt::format("PATCH-{}", static_cast<int>(e));
  registry.emplace<hf::meta>(e, meta);

  wl::transform transform;
  transform.position.x = static_cast<float>(origin_x * 25);
  transform.position.y = static_cast<float>(origin_y * 25);
  registry.emplace<wl::transform>(e, transform);

  ResourcePatch patch;
  patch.patch_type = patch_type;
  patch.item_name = def->item;
  patch.obstacle = def->obstacle;
  patch.cells = std::move(cells);
  patch.recalculateBounds();
  registry.emplace<ResourcePatch>(e, patch);

  // Parent to patches folder
  auto& wk = entt::locator<WellKnownEntities>::value();
  if (wk.patches_folder != entt::null && registry.valid(wk.patches_folder)) {
    auto& rel = registry.get_or_emplace<wl::relation>(e);
    rel.parent = wk.patches_folder;
    auto& prel = registry.get_or_emplace<wl::relation>(wk.patches_folder);
    if (std::find(prel.children.begin(), prel.children.end(), e) == prel.children.end()) {
      prel.children.push_back(e);
    }
  }

  // Track for spacing
  int cx = origin_x, cy = origin_y;
  if (!patch.cells.empty()) {
    int sx = 0, sy = 0;
    for (const auto& [px, py] : patch.cells) { sx += px; sy += py; }
    cx = sx / static_cast<int>(patch.cells.size());
    cy = sy / static_cast<int>(patch.cells.size());
  }
  placed_.push_back({patch_type, cx, cy});
  markOccupied(patch.cells);

  return e;
}

// ── Stage 1: Biome layout ───────────────────────────────────────────────────

void MapGenerator::generateBiomeLayout(const MapGenConfig& config,
                                       const std::vector<BiomeDef>& biomes) {
  if (biomes.empty()) return;

  int n_seeds = std::max(1, config.biome_seed_count);
  int n_biomes = static_cast<int>(biomes.size());

  // Generate Voronoi seed points
  struct Seed { int x, y, biome_idx; };
  std::vector<Seed> seeds;
  std::uniform_int_distribution<int> x_dis(0, map_w_ - 1);
  std::uniform_int_distribution<int> y_dis(0, map_h_ - 1);
  std::uniform_int_distribution<int> b_dis(0, n_biomes - 1);

  for (int i = 0; i < n_seeds; ++i) {
    seeds.push_back({x_dis(rng_), y_dis(rng_), b_dis(rng_)});
  }

  // Assign each cell to nearest seed (Voronoi)
  for (int y = 0; y < map_h_; ++y) {
    for (int x = 0; x < map_w_; ++x) {
      float min_dist = 1e9f;
      int best = 0;
      for (int s = 0; s < static_cast<int>(seeds.size()); ++s) {
        float dx = static_cast<float>(x - seeds[s].x);
        float dy = static_cast<float>(y - seeds[s].y);
        float d = dx * dx + dy * dy; // squared distance is fine for comparison
        if (d < min_dist) {
          min_dist = d;
          best = s;
        }
      }
      biome_grid_[y][x] = seeds[best].biome_idx;
    }
  }

  // Optional: CA smoothing pass to make borders more organic
  for (int pass = 0; pass < 2; ++pass) {
    auto next = biome_grid_;
    for (int y = 1; y < map_h_ - 1; ++y) {
      for (int x = 1; x < map_w_ - 1; ++x) {
        // Count neighbor biome votes
        std::unordered_map<int, int> votes;
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            votes[biome_grid_[y + dy][x + dx]]++;
          }
        }
        // Majority wins
        int best_biome = biome_grid_[y][x];
        int best_count = 0;
        for (const auto& [b, c] : votes) {
          if (c > best_count) { best_count = c; best_biome = b; }
        }
        next[y][x] = best_biome;
      }
    }
    biome_grid_ = next;
  }
}

// ── Stage 2: Starter patches ────────────────────────────────────────────────

void MapGenerator::placeStarterPatches(const MapGenConfig& config,
                                       entt::registry& registry,
                                       nlohmann::json& summary) {
  // Mark the starter zone as occupied (keep it clear)
  for (int y = 0; y < map_h_; ++y) {
    for (int x = 0; x < map_w_; ++x) {
      if (isInStarterZone(x, y, config.starter_radius)) {
        occupancy_[y][x] = true;
      }
    }
  }

  // Place each starter patch at the configured distance from center
  for (const auto& sp : config.starter_patches) {
    float dist = sp.distance;
    // Try several angles to find a good placement
    bool placed = false;
    for (int attempt = 0; attempt < config.max_placement_attempts; ++attempt) {
      std::uniform_real_distribution<float> angle_dis(0.0f, 6.283185f);
      float angle = angle_dis(rng_);
      int ox = center_x_ + static_cast<int>(dist * std::cos(angle));
      int oy = center_y_ + static_cast<int>(dist * std::sin(angle));
      ox = std::clamp(ox, 0, map_w_ - 1);
      oy = std::clamp(oy, 0, map_h_ - 1);

      auto cells = generateBlobAtPosition(sp.type, ox, oy);
      if (cells.empty()) continue;

      float overlap = overlapFraction(cells);
      if (overlap < config.max_overlap_fraction) {
        createPatch(registry, sp.type, ox, oy, std::move(cells));
        summary[sp.type] = summary.value(sp.type, 0) + 1;
        placed = true;
        break;
      }
    }
    if (!placed) {
      mapgen_log.warn("Could not place starter patch: {}", sp.type);
    }
  }
}

// ── Stage 3: Obstacles ──────────────────────────────────────────────────────

void MapGenerator::placeObstacles(const MapGenConfig& config,
                                  const std::vector<BiomeDef>& biomes,
                                  entt::registry& registry,
                                  nlohmann::json& summary) {
  float map_radius = std::sqrt(static_cast<float>(map_w_ * map_w_ + map_h_ * map_h_)) / 2.0f;

  for (const auto& obs_def : config.base_obstacles) {
    std::uniform_int_distribution<int> count_dis(obs_def.count_min, obs_def.count_max);
    int target_count = count_dis(rng_);

    for (int i = 0; i < target_count; ++i) {
      bool placed = false;
      for (int attempt = 0; attempt < config.max_placement_attempts; ++attempt) {
        std::uniform_int_distribution<int> x_dis(0, map_w_ - 1);
        std::uniform_int_distribution<int> y_dis(0, map_h_ - 1);
        int ox = x_dis(rng_);
        int oy = y_dis(rng_);

        // Skip if in starter zone
        if (isInStarterZone(ox, oy, config.starter_radius + 5)) continue;

        // Check biome obstacle weight
        if (!biomes.empty() && oy >= 0 && oy < map_h_ && ox >= 0 && ox < map_w_) {
          int biome_idx = biome_grid_[oy][ox];
          if (biome_idx >= 0 && biome_idx < static_cast<int>(biomes.size())) {
            const auto& biome = biomes[biome_idx];
            auto it = biome.obstacle_weights.find(obs_def.patch_type);
            float weight = (it != biome.obstacle_weights.end()) ? it->second : 0.5f;
            weight *= biome.obstacle_density;
            // Probabilistic rejection based on weight
            std::uniform_real_distribution<float> prob(0.0f, 2.0f);
            if (prob(rng_) > weight) continue;
          }
        }

        auto cells = generateBlobAtPosition(obs_def.patch_type, ox, oy);
        if (cells.empty()) continue;

        float overlap = overlapFraction(cells);
        if (overlap < config.max_overlap_fraction) {
          createPatch(registry, obs_def.patch_type, ox, oy, std::move(cells));
          summary[obs_def.patch_type] = summary.value(obs_def.patch_type, 0) + 1;
          placed = true;
          break;
        }
      }
    }
  }
}

// ── Stage 4: Resources ──────────────────────────────────────────────────────

void MapGenerator::placeResources(const MapGenConfig& config,
                                  const std::vector<BiomeDef>& biomes,
                                  entt::registry& registry,
                                  nlohmann::json& summary) {
  for (const auto& res_def : config.base_resources) {
    std::uniform_int_distribution<int> count_dis(res_def.count_min, res_def.count_max);
    int target_count = count_dis(rng_);

    for (int i = 0; i < target_count; ++i) {
      bool placed = false;
      for (int attempt = 0; attempt < config.max_placement_attempts; ++attempt) {
        std::uniform_int_distribution<int> x_dis(0, map_w_ - 1);
        std::uniform_int_distribution<int> y_dis(0, map_h_ - 1);
        int ox = x_dis(rng_);
        int oy = y_dis(rng_);

        // Check biome resource weight
        if (!biomes.empty() && oy >= 0 && oy < map_h_ && ox >= 0 && ox < map_w_) {
          int biome_idx = biome_grid_[oy][ox];
          if (biome_idx >= 0 && biome_idx < static_cast<int>(biomes.size())) {
            const auto& biome = biomes[biome_idx];
            auto it = biome.resource_weights.find(res_def.patch_type);
            float weight = (it != biome.resource_weights.end()) ? it->second : 0.3f;
            // Probabilistic acceptance based on weight
            std::uniform_real_distribution<float> prob(0.0f, 2.0f);
            if (prob(rng_) > weight) continue;
          }
        }

        // Enforce minimum same-type spacing
        auto cells = generateBlobAtPosition(res_def.patch_type, ox, oy);
        if (cells.empty()) continue;

        // Compute centroid for spacing check
        int sx = 0, sy = 0;
        for (const auto& [px, py] : cells) { sx += px; sy += py; }
        int cx = sx / static_cast<int>(cells.size());
        int cy = sy / static_cast<int>(cells.size());

        if (minDistanceToSameType(res_def.patch_type, cx, cy) < config.min_same_type_distance) {
          continue;
        }

        float overlap = overlapFraction(cells);
        if (overlap < config.max_overlap_fraction) {
          createPatch(registry, res_def.patch_type, ox, oy, std::move(cells));
          summary[res_def.patch_type] = summary.value(res_def.patch_type, 0) + 1;
          placed = true;
          break;
        }
      }
    }
  }
}

// ── Stage 5: Lua features ───────────────────────────────────────────────────

void MapGenerator::executeFeatures(const std::vector<MapGenFeatureDef>& features,
                                   const MapGenConfig& config,
                                   entt::registry& registry,
                                   sol::state& lua,
                                   nlohmann::json& summary) {
  if (features.empty()) return;

  // Build context table for Lua features
  sol::table ctx = lua.create_table();
  ctx["width"] = map_w_;
  ctx["height"] = map_h_;
  ctx["center_x"] = center_x_;
  ctx["center_y"] = center_y_;

  // RNG functions
  ctx["rng_int"] = [this](int lo, int hi) -> int {
    std::uniform_int_distribution<int> dis(lo, hi);
    return dis(rng_);
  };
  ctx["rng_float"] = [this](float lo, float hi) -> float {
    std::uniform_real_distribution<float> dis(lo, hi);
    return dis(rng_);
  };

  // Occupancy query
  ctx["is_occupied"] = [this](int x, int y) -> bool {
    if (x < 0 || x >= map_w_ || y < 0 || y >= map_h_) return true;
    return occupancy_[y][x];
  };

  // Biome query
  ctx["get_biome"] = [this](int x, int y) -> int {
    if (x < 0 || x >= map_w_ || y < 0 || y >= map_h_) return -1;
    return biome_grid_[y][x];
  };

  // Place patch helper
  ctx["place_patch"] = [this, &registry, &summary](const std::string& patch_type, int x, int y) -> bool {
    auto cells = generateBlobAtPosition(patch_type, x, y);
    if (cells.empty()) return false;
    if (overlapFraction(cells) > 0.5f) return false;
    createPatch(registry, patch_type, x, y, std::move(cells));
    summary[patch_type] = summary.value(patch_type, 0) + 1;
    return true;
  };

  for (const auto& feature : features) {
    if (!feature.place.valid()) continue;
    auto result = feature.place(ctx);
    if (!result.valid()) {
      sol::error err = result;
      mapgen_log.warn("Feature '{}' error: {}", feature.key, err.what());
    }
  }
}

// ── Main generate ───────────────────────────────────────────────────────────

MapGenResult MapGenerator::generate(const MapGenConfig& config,
                                    const std::vector<BiomeDef>& biomes,
                                    const std::vector<MapGenFeatureDef>& features,
                                    entt::registry& registry,
                                    sol::state& lua) {
  MapGenResult result;

  // Seed
  uint64_t seed = config.seed;
  if (seed == 0) {
    std::random_device rd;
    seed = rd();
  }
  rng_.seed(static_cast<std::mt19937::result_type>(seed));
  result.seed_used = seed;

  mapgen_log.info("Generating map {}x{} with seed {}", config.width, config.height, seed);

  // Stage 0: Init
  initGrids(config.width, config.height);

  // Stage 1: Biome layout
  mapgen_log.info("Stage 1: Biome layout ({} biomes, {} seeds)",
                  biomes.size(), config.biome_seed_count);
  generateBiomeLayout(config, biomes);
  for (const auto& b : biomes) {
    result.biomes_used.push_back(b.key);
  }

  // Stage 2: Starter patches
  mapgen_log.info("Stage 2: Starter area (radius {})", config.starter_radius);
  placeStarterPatches(config, registry, result.summary);

  // Execute stage <= 2 features
  std::vector<MapGenFeatureDef> early_features, mid_features, late_features;
  for (const auto& f : features) {
    if (f.stage <= 2) early_features.push_back(f);
    else if (f.stage <= 4) mid_features.push_back(f);
    else late_features.push_back(f);
  }
  executeFeatures(early_features, config, registry, lua, result.summary);

  // Stage 3: Obstacles
  mapgen_log.info("Stage 3: Obstacles");
  placeObstacles(config, biomes, registry, result.summary);
  executeFeatures(mid_features, config, registry, lua, result.summary);

  // Stage 4: Resources
  mapgen_log.info("Stage 4: Resources");
  placeResources(config, biomes, registry, result.summary);

  // Stage 5: Late features
  mapgen_log.info("Stage 5: Features");
  executeFeatures(late_features, config, registry, lua, result.summary);

  // Count total
  for (auto& [type, count] : result.summary.items()) {
    result.patches_placed += count.get<int>();
  }

  mapgen_log.info("Map generation complete: {} patches placed", result.patches_placed);
  return result;
}
