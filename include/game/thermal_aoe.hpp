#pragma once

#include <game/components/frame.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include <utils/entt.hpp>

struct Environment;

namespace warlock::thermal_aoe {

inline constexpr const char *kExternalAoCooler = "External AO Cooler";
inline constexpr const char *kExternalAoHeater = "External AO Heater";

inline float airflowMultiplier(float envAirflow) {
  return 0.1f + 0.02f * envAirflow;
}

bool isExternalAoeType(const std::string &t);

bool isInternallyThermalControlled(const std::string &t);

void computeFrameAoeBias(entt::registry &registry, Environment *env,
                         std::unordered_map<entt::entity, float> &out);

/// Scalar field: environment.temperature + Σ(strength * falloff * airflowMultiplier).
void fillThermalFieldRect(entt::registry &registry, Environment *env, int minGx, int minGy,
                          int width, int height, std::vector<float> &out);

} // namespace warlock::thermal_aoe
