#include <game/mapgen_loader.hpp>
#include <algorithm>
#include <filesystem>
#include <fmt/core.h>

namespace fs = std::filesystem;

namespace {

int luaInt(sol::table t, const char* key, int def) {
  sol::object o = t[key];
  if (!o.valid()) return def;
  return static_cast<int>(o.as<double>());
}

float luaFloat(sol::table t, const char* key, float def) {
  sol::object o = t[key];
  if (!o.valid()) return def;
  return static_cast<float>(o.as<double>());
}

uint8_t luaU8(sol::table t, const char* key, uint8_t def) {
  sol::object o = t[key];
  if (!o.valid()) return def;
  return static_cast<uint8_t>(o.as<int>());
}

} // namespace

void MapGenLoader::load(const std::string& mapgen_path, sol::state& lua) {
  config_ = {};
  biomes_.clear();
  features_.clear();

  loadConfig(mapgen_path, lua);

  auto biomes_dir = fs::path(mapgen_path) / "biomes";
  if (fs::exists(biomes_dir)) {
    loadBiomes(biomes_dir.string(), lua);
  }

  auto features_dir = fs::path(mapgen_path) / "features";
  if (fs::exists(features_dir)) {
    loadFeatures(features_dir.string(), lua);
  }

  // Sort features by stage
  std::sort(features_.begin(), features_.end(),
            [](const MapGenFeatureDef& a, const MapGenFeatureDef& b) {
              return a.stage < b.stage;
            });
}

void MapGenLoader::loadConfig(const std::string& path, sol::state& lua) {
  auto config_file = fs::path(path) / "config.lua";
  if (!fs::exists(config_file)) return;

  sol::table t = lua.load_file(config_file.string()).call();

  config_.width = luaInt(t, "width", 200);
  config_.height = luaInt(t, "height", 200);
  config_.seed = static_cast<uint64_t>(luaFloat(t, "seed", 0));
  config_.starter_radius = luaInt(t, "starter_radius", 10);
  config_.biome_seed_count = luaInt(t, "biome_seed_count", 5);
  config_.max_overlap_fraction = luaFloat(t, "max_overlap_fraction", 0.3f);
  config_.max_placement_attempts = luaInt(t, "max_placement_attempts", 50);
  config_.min_same_type_distance = luaFloat(t, "min_same_type_distance", 20.0f);

  // Starter patches
  sol::object starter_obj = t["starter"];
  if (starter_obj.valid() && starter_obj.get_type() == sol::type::table) {
    sol::table starter = starter_obj;
    for (auto& [idx, val] : starter) {
      if (val.get_type() != sol::type::table) continue;
      sol::table entry = val;
      MapGenStarterPatch sp;
      sp.type = entry.get_or<std::string>("type", "");
      sp.distance = luaFloat(entry, "distance", 12.0f);
      if (!sp.type.empty()) {
        config_.starter_patches.push_back(sp);
      }
    }
  }

  // Base resources
  sol::object res_obj = t["base_resources"];
  if (res_obj.valid() && res_obj.get_type() == sol::type::table) {
    sol::table res = res_obj;
    for (auto& [key, val] : res) {
      if (val.get_type() != sol::type::table) continue;
      MapGenResourceDef rd;
      rd.patch_type = key.as<std::string>();
      sol::table entry = val;
      sol::object count_obj = entry["count"];
      if (count_obj.valid() && count_obj.get_type() == sol::type::table) {
        sol::table count = count_obj;
        rd.count_min = luaInt(count, "1", 2);
        rd.count_max = luaInt(count, "2", 5);
        // Lua tables are 1-indexed, sol accesses by key
        sol::object c1 = count[1];
        sol::object c2 = count[2];
        if (c1.valid()) rd.count_min = static_cast<int>(c1.as<double>());
        if (c2.valid()) rd.count_max = static_cast<int>(c2.as<double>());
      }
      config_.base_resources.push_back(rd);
    }
  }

  // Base obstacles
  sol::object obs_obj = t["base_obstacles"];
  if (obs_obj.valid() && obs_obj.get_type() == sol::type::table) {
    sol::table obs = obs_obj;
    for (auto& [key, val] : obs) {
      if (val.get_type() != sol::type::table) continue;
      MapGenResourceDef rd;
      rd.patch_type = key.as<std::string>();
      sol::table entry = val;
      sol::object count_obj = entry["count"];
      if (count_obj.valid() && count_obj.get_type() == sol::type::table) {
        sol::table count = count_obj;
        sol::object c1 = count[1];
        sol::object c2 = count[2];
        if (c1.valid()) rd.count_min = static_cast<int>(c1.as<double>());
        if (c2.valid()) rd.count_max = static_cast<int>(c2.as<double>());
      }
      config_.base_obstacles.push_back(rd);
    }
  }
}

void MapGenLoader::loadBiomes(const std::string& path, sol::state& lua) {
  for (const auto& entry : fs::directory_iterator(path)) {
    if (entry.path().extension() != ".lua") continue;

    std::string key = entry.path().stem().string();
    sol::table t = lua.load_file(entry.path().string()).call();

    BiomeDef biome;
    biome.key = key;
    biome.name = t.get_or<std::string>("name", key);
    biome.obstacle_density = luaFloat(t, "obstacle_density", 1.0f);

    // Tint color
    sol::object tint_obj = t["tint"];
    if (tint_obj.valid() && tint_obj.get_type() == sol::type::table) {
      sol::table tint = tint_obj;
      biome.tint.r = luaU8(tint, "r", 128);
      biome.tint.g = luaU8(tint, "g", 128);
      biome.tint.b = luaU8(tint, "b", 128);
      biome.tint.a = luaU8(tint, "a", 30);
    }

    // Resource weights
    sol::object rw_obj = t["resource_weights"];
    if (rw_obj.valid() && rw_obj.get_type() == sol::type::table) {
      sol::table rw = rw_obj;
      for (auto& [k, v] : rw) {
        biome.resource_weights[k.as<std::string>()] = static_cast<float>(v.as<double>());
      }
    }

    // Obstacle weights
    sol::object ow_obj = t["obstacle_weights"];
    if (ow_obj.valid() && ow_obj.get_type() == sol::type::table) {
      sol::table ow = ow_obj;
      for (auto& [k, v] : ow) {
        biome.obstacle_weights[k.as<std::string>()] = static_cast<float>(v.as<double>());
      }
    }

    biomes_.push_back(biome);
  }
}

void MapGenLoader::loadFeatures(const std::string& path, sol::state& lua) {
  for (const auto& entry : fs::directory_iterator(path)) {
    if (entry.path().extension() != ".lua") continue;

    std::string key = entry.path().stem().string();
    sol::table t = lua.load_file(entry.path().string()).call();

    MapGenFeatureDef feature;
    feature.key = key;
    feature.name = t.get_or<std::string>("name", key);
    feature.stage = luaInt(t, "stage", 5);

    sol::object place_obj = t["place"];
    if (place_obj.valid() && place_obj.get_type() == sol::type::function) {
      feature.place = place_obj;
    }

    features_.push_back(feature);
  }
}
