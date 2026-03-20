#include <game/patch_loader.hpp>
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

} // namespace

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
    {
      const sol::object o = t["obstacle"];
      if (o.valid()) {
        if (o.get_type() == sol::type::boolean) {
          def.obstacle = o.as<bool>();
        } else if (o.get_type() == sol::type::number) {
          def.obstacle = o.as<int>() != 0;
        }
      }
    }
    
    if (sol::table color = t["color"]; color.valid()) {
      def.color.r = color.get_or<uint8_t>("r", 255);
      def.color.g = color.get_or<uint8_t>("g", 200);
      def.color.b = color.get_or<uint8_t>("b", 50);
      def.color.a = color.get_or<uint8_t>("a", 100);
    }
    
    if (sol::table gen = t["generation"]; gen.valid()) {
      def.generation.min_width = luaInt(gen, "min_width", 3);
      def.generation.max_width = luaInt(gen, "max_width", 8);
      def.generation.min_height = luaInt(gen, "min_height", 3);
      def.generation.max_height = luaInt(gen, "max_height", 8);
      def.generation.fill_probability = luaFloat(gen, "fill_probability", 0.35f);
      def.generation.smoothing_rounds = luaInt(gen, "smoothing_rounds", 5);
      auto clampWH = [](int a, int b) {
        const int lo = std::max(1, std::min(a, b));
        const int hi = std::max(lo, std::max(a, b));
        return std::pair<int, int>{lo, hi};
      };
      {
        const auto w = clampWH(def.generation.min_width, def.generation.max_width);
        def.generation.min_width = w.first;
        def.generation.max_width = w.second;
      }
      {
        const auto h = clampWH(def.generation.min_height, def.generation.max_height);
        def.generation.min_height = h.first;
        def.generation.max_height = h.second;
      }
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
