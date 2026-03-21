#include <functional>

#include <game/frame_deposit_query.hpp>
#include <game/thermal_aoe.hpp>
#include <game/thermal_grid.hpp>

namespace tg = warlock::thermal_grid;

namespace warlock::thermal_aoe {

bool isExternalAoeType(const std::string &t) {
  return t == kExternalAoCooler || t == kExternalAoHeater;
}

bool isInternallyThermalControlled(const std::string &t) {
  return t == "Temp Control" || isExternalAoeType(t);
}

static void forEachActiveExternalAoe(
    entt::registry &registry, Environment *env,
    const std::function<void(float ax, float ay, float R, float strength)> &fn) {
  if (!env) {
    return;
  }
  const float fAir = airflowMultiplier(env->airFlow);

  for (auto se : registry.view<Frame, wl::transform>()) {
    auto &sframe = registry.get<Frame>(se);
    auto &str = registry.get<wl::transform>(se);
    const auto scells = frameOccupiedGridCells(str, sframe.size);
    const auto anchor = tg::anchorFromOccupiedCells(scells);
    const float ax = anchor.first;
    const float ay = anchor.second;

    for (const auto &comp : sframe.components) {
      if (comp->state != ComponentState::ACTIVE) {
        continue;
      }
      if (!comp->data.has("type")) {
        continue;
      }
      const std::string ty = comp->data.get<std::string>("type");
      if (!isExternalAoeType(ty)) {
        continue;
      }
      const int R = comp->data.get_or<int>("radius", 0);
      const float strength = comp->data.get_or<float>("heat", 0.f);
      if (R <= 0) {
        continue;
      }
      fn(ax, ay, static_cast<float>(R), strength * fAir);
    }
  }
}

void computeFrameAoeBias(entt::registry &registry, Environment *env,
                         std::unordered_map<entt::entity, float> &out) {
  out.clear();
  for (auto e : registry.view<Frame, wl::transform>()) {
    out[e] = 0.f;
  }

  forEachActiveExternalAoe(registry, env, [&](float ax, float ay, float R, float strength) {
    for (auto te : registry.view<Frame, wl::transform>()) {
      auto &tframe = registry.get<Frame>(te);
      auto &ttr = registry.get<wl::transform>(te);
      const auto tcells = frameOccupiedGridCells(ttr, tframe.size);
      const float avgW = tg::averageFalloffToCells(ax, ay, R, tcells);
      if (avgW <= 0.f) {
        continue;
      }
      out[te] += strength * avgW;
    }
  });
}

void fillThermalFieldRect(entt::registry &registry, Environment *env, int minGx, int minGy,
                          int width, int height, std::vector<float> &out) {
  const size_t n = static_cast<size_t>(width) * static_cast<size_t>(height);
  out.assign(n, env ? env->temperature : 0.f);
  if (!env || width <= 0 || height <= 0) {
    return;
  }

  forEachActiveExternalAoe(registry, env, [&](float ax, float ay, float R, float strength) {
    for (int iy = 0; iy < height; ++iy) {
      const int gy = minGy + iy;
      for (int ix = 0; ix < width; ++ix) {
        const int gx = minGx + ix;
        const float w = tg::falloffAtCell(ax, ay, R, gx, gy);
        if (w <= 0.f) {
          continue;
        }
        const size_t idx = static_cast<size_t>(iy) * static_cast<size_t>(width) +
                           static_cast<size_t>(ix);
        out[idx] += strength * w;
      }
    }
  });
}

} // namespace warlock::thermal_aoe
