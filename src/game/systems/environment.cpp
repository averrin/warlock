#include <fmt/core.h>
#include <fmt/ranges.h>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <game/systems/environment.hpp>
#include <liblog/liblog.hpp>
#include <ranges> // For ranges
#include <utils/entt.hpp>
#include <effolkronium/random.hpp>
using Random = effolkronium::random_static;


void EnvironmentSystem::onNewDay(Environment& environment) {
  auto max_temp = randomTemp(70.0f, 120.0f);
  auto min_temp = randomTemp(-70.0f, -120.0f);
  temp_tween = tweeny::from(min_temp)
                   .to(max_temp)
                   .during(60.0f*12.0f)
                   .via(easing::sinusoidalInOut)
                   .to(min_temp)
                   .during(60.0f*12.0f)
                   .via(easing::sinusoidalInOut);

  environment.airFlow = randomTemp(-10.0f, 20.0f);
  if (environment.airFlow < 0) {
    environment.airFlow = 0;
  }
}

void EnvironmentSystem::fixedUpdate() {
  auto max_minutes = 1440;

  auto &current_state = entt::locator<State>::value();
  for (auto &e : current_state.registry.view<Environment>()) {
    auto &environment = current_state.registry.get<Environment>(e);

    if (!ready) {
      onNewDay(environment);
      ready = true;
    }

    environment.minutes += 1;
    if (environment.minutes >= max_minutes) {
      environment.minutes = 0;
      onNewDay(environment);
    }
    temp_tween.step(1);
    environment.temperature = temp_tween.peek();

    break;
  }
}
