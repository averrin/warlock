#include <fmt/core.h>
#include <fmt/ranges.h>
#include <game/components/frame.hpp>
#include <game/well_known_entities.hpp>
#include <game/state.hpp>
#include <game/systems/thermal.hpp>
#include <liblog/liblog.hpp>
#include <ranges> // For ranges
#include <utils/entt.hpp>

void ThermalSystem::fixedUpdate() {
  auto &current_state = entt::locator<State>::value();
  auto &wk = entt::locator<WellKnownEntities>::value();
  if (wk.environment == entt::null || !current_state.registry.valid(wk.environment) ||
      !current_state.registry.all_of<Environment>(wk.environment)) {
    return;
  }
  environment = &current_state.registry.get<Environment>(wk.environment);
  environment->temperatures[-1].push_back(environment->temperature);
  if (environment->temperatures[-1].size() > 100) {
    environment->temperatures[-1].pop_front();
  }

  for (auto &f : current_state.registry.view<Frame>()) {
    auto &frame = current_state.registry.get<Frame>(f);
    auto surfaceArea = surfaceAreas[frame.size];

    auto acTemp = 0.0f;
    for (auto &component : frame.components) {
      if (component->data.get<std::string>("type") == "Temp Control" &&
          component->state == ComponentState::ACTIVE) {
        acTemp += component->data.get_or<float>("heat", 0.0f);
      }
    }
    for (auto &component : frame.components) {
      float totalHeatTransfer = 0.0;
      auto volume = volumes[component->size];
      auto temp = component->data.get<float>("temp");
      if (temp <= -1000.f) {
        temp = environment->temperature;
      }

      if (component->data.get<std::string>("type") != "Temp Control") {
        if (component->state != ComponentState::DEACTIVATED &&
            component->state != ComponentState::BROKEN &&
            component->state != ComponentState::DESTROYED) {
          auto heat = component->data.get_or<float>("heat", 0.0f);
          temp += heat;
        }
        temp += acTemp;
      }

      for (auto other : frame.components) {
        if (other != component) {
          auto o_volume = volumes[other->size];
          float contactArea = std::min(volume, o_volume) /
                              std::max(volume, o_volume) * surfaceArea;
          totalHeatTransfer +=
              calculateConduction(component.get(), other.get(), contactArea);
        }
      }

      // Convection and radiation with environment
      float componentSurfaceArea = std::pow(volume, 2.0f / 3.0f);
      float heatTransferCoeff =
          10.0f + 2.0f * environment->airFlow; // Simplified coefficient
      totalHeatTransfer +=
          heatTransferCoeff * surfaceArea * (environment->temperature - temp);

      totalHeatTransfer +=
          calculateRadiation(component.get(), componentSurfaceArea);

      float temperatureChange =
          totalHeatTransfer / getThermalMass(component.get()) * timeStep;
      component->data.set<float>("temp", temp + temperatureChange);

      if (component->data.get_or<bool>("passive", false) ||
          component->data.get_or<bool>("temp_proof", false)) {
      } else {
        auto eff = component->data.attributes["efficiency"];
        if (component->data.get<float>("temp") > heat_effect_temp) {
          component->data.addEffect(EffectID::OVERHEAT);
          eff->AddModifier(effects[EffectID::OVERHEAT]);
        } else {
          component->data.removeEffect(EffectID::OVERHEAT);
          eff->RemoveModifier(effects[EffectID::OVERHEAT]);
        }

        auto consumption = component->data.attributes.find("consumption");
        if (component->data.get<float>("temp") < cold_effect_temp) {
          component->data.addEffect(EffectID::FREEZE);
          if (consumption != component->data.attributes.end()) {
            consumption->second->AddModifier(effects[EffectID::FREEZE]);
          }
        } else {
          component->data.removeEffect(EffectID::FREEZE);
          if (consumption != component->data.attributes.end()) {
            consumption->second->RemoveModifier(effects[EffectID::FREEZE]);
          }
        }

        if (component->state == ComponentState::ACTIVE &&
            (component->data.get<float>("temp") > max_break_temp ||
             component->data.get<float>("temp") < min_break_temp)) {
          auto prev = component->state;
          auto curTemp = component->data.get<float>("temp");
          component->state = ComponentState::BROKEN;
          component->error = fmt::format("temperature {:.1f}°C (limit {:.0f}–{:.0f})",
                                         curTemp, min_break_temp, max_break_temp);
          auto &emitter = entt::locator<event_emitter>::value();
          emitter.publish(component_state_changed{
            frame.data.id, component->data.id, component->data.name,
            static_cast<int>(prev), static_cast<int>(component->state),
            component->error
          });
        }
      }

      environment->temperatures[component->data.id].push_back(
          temp + temperatureChange);

      if (environment->temperatures[component->data.id].size() > 100) {
        environment->temperatures[component->data.id].pop_front();
      }
    }
  }
}
