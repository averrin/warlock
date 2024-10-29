#include <effolkronium/random.hpp>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <game/systems/environment.hpp>
#include <liblog/liblog.hpp>
#include <ranges> // For ranges
#include <utils/entt.hpp>
using Random = effolkronium::random_static;

void EnvironmentSystem::onNewDay(Environment &environment) {
  auto abs_max_temp = 120.0f;
  auto abs_min_temp = -200.0f;
  auto top_temp_diff = 50.0f;
  auto bottom_temp_diff = 50.0f;
  auto max_sun = randomNormalFloat(25.0f, 100.0f);
  auto max_temp = randomNormalFloat(abs_max_temp - top_temp_diff, abs_max_temp);
  auto min_temp =
      randomNormalFloat(abs_min_temp, abs_min_temp + bottom_temp_diff);
  temp_tween = tweeny::from(min_temp)
                   .to(max_temp)
                   .during(60.0f * 12.0f)
                   .via(easing::sinusoidalInOut)
                   .to(min_temp)
                   .during(60.0f * 12.0f)
                   .via(easing::sinusoidalInOut);
  sun_tween = tweeny::from(0.0f)
                  .to(max_sun)
                  .during(60.0f * 6.0f)
                  .via(easing::sinusoidalInOut)
                  .to(0.0f)
                  .during(60.0f * 6.0f)
                  .via(easing::sinusoidalInOut);
  environment.airFlow = randomNormalFloat(-10.0f, 20.0f);
  if (environment.airFlow < 0) {
    environment.airFlow = 0;
  }
  environment.days += 1;
}

void EnvironmentSystem::fixedUpdate() {
  auto max_minutes = 1440;

  auto &current_state = entt::locator<State>::value();
  auto &environment = current_state.registry.get<Environment>((entt::entity)0);

  if (!ready) {
    onNewDay(environment);
    ready = true;
  }

  environment.minutes += 1;
  environment.isDay =
      environment.minutes > 1440 / 4 && environment.minutes < 1440 / 4 * 3;
  if (!environment.isDay) {
    environment.sun = 0.0f;
  } else {
    sun_tween.step(1);
    environment.sun = sun_tween.peek();
  }
  if (environment.minutes >= max_minutes) {
    environment.minutes = 0;
    onNewDay(environment);
  }
  if (environment.minutes % 60 == 0 && Random::get<bool>(0.25)) {
    environment.airFlow = randomNormalFloat(-10.0f, 20.0f);
    if (environment.airFlow < 0) {
      environment.airFlow = 0;
    }
  }
  temp_tween.step(1);
  environment.temperature = temp_tween.peek();

  environment.named_history["sun"].push_back(environment.sun);
  if (environment.named_history["sun"].size() > 100) {
    environment.named_history["sun"].pop_front();
  }
  environment.named_history["airFlow"].push_back(environment.airFlow);
  if (environment.named_history["airFlow"].size() > 100) {
    environment.named_history["airFlow"].pop_front();
  }

  for (auto &f : current_state.registry.view<Frame>()) {
    auto frame = current_state.registry.get<Frame>(f);
    for (auto &component : frame.components) {
      if (component->data.get<std::string>("type") == "Solar" &&
          component->state == ComponentState::ACTIVE) {
        component->data.set<float>("efficiency", environment.sun / 100.0f);
      }
    }
  }
}
