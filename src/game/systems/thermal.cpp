#include <fmt/core.h>
#include <fmt/ranges.h>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <game/systems/thermal.hpp>
#include <liblog/liblog.hpp>
#include <ranges> // For ranges
#include <utils/entt.hpp>

void ThermalSystem::fixedUpdate() {

  auto heat_effect_temp = 150.0f;
  auto cold_effect_temp = -50.0f;

  auto &current_state = entt::locator<State>::value();
  for (auto &e : current_state.registry.view<Environment>()) {
    environment = &current_state.registry.get<Environment>(e);
    environment->temperatures[-1].push_back(environment->temperature);
      if (environment->temperatures[-1].size() > 100) {
        environment->temperatures[-1].pop_front();
      }
    break;
  }

  for (auto &f : current_state.registry.view<Frame>()) {
    auto frame = current_state.registry.get<Frame>(f);
    auto surfaceArea = surfaceAreas[frame.size];
    for (auto &component : frame.components) {
      float totalHeatTransfer = 0.0;
      auto volume = volumes[component->size];
      auto temp = component->data.get<float>("temp");
      if (temp <= -1000.f) {
        temp = environment->temperature;
      }
      if (component->state != ComponentState::DEACTIVATED) {
        auto heat = component->data.get_or<float>("heat", 0.0f);
        temp += heat;
      }

      for (auto other : frame.components) {
          if (other != component) {
            auto o_volume = volumes[other->size];
              float contactArea = std::min(volume, o_volume) / 
                                   std::max(volume, o_volume) * surfaceArea;
              totalHeatTransfer += calculateConduction(component.get(), other.get(), contactArea);
          }
      }

      // Convection and radiation with environment
      float componentSurfaceArea = std::pow(volume, 2.0f/3.0f);
      float heatTransferCoeff = 10.0f + 2.0f * environment->airFlow;  // Simplified coefficient
      totalHeatTransfer += heatTransferCoeff * surfaceArea * (environment->temperature - temp);

      totalHeatTransfer += calculateRadiation(component.get(), componentSurfaceArea);

      float temperatureChange = totalHeatTransfer / getThermalMass(component.get()) * timeStep;
      component->data.set<float>("temp", temp + temperatureChange);


      if (component->data.get<float>("temp") > heat_effect_temp) {
        component->data.addEffect(EffectID::OVERHEAT);
      } else {
        component->data.removeEffect(EffectID::OVERHEAT);
      }

      if (component->data.get<float>("temp") < cold_effect_temp) {
        component->data.addEffect(EffectID::FREEZE);
      }else{
        component->data.removeEffect(EffectID::FREEZE);
      }

      environment->temperatures[component->data.id].push_back(temp + temperatureChange);

      if (environment->temperatures[component->data.id].size() > 100) {
        environment->temperatures[component->data.id].pop_front();
      }
    }
  }
}
