#include <rpc/event_bridge.hpp>
#include <utils/entt.hpp>

namespace rpc {

void initEventBridge(Server& server) {
  auto& emitter = entt::locator<event_emitter>::value();

  // Bridge init_event -> notify.state.changed
  emitter.connect<init_event>(
    std::function<void(init_event&, const event_emitter&)>(
      [&server](init_event& event, const event_emitter&) {
        server.broadcast("notify.state.changed", {
          {"reason", "init"},
          {"component", event.component}
        });
      }
    )
  );

  // Bridge ready_event -> notify.game.started
  emitter.connect<ready_event>(
    std::function<void(ready_event&, const event_emitter&)>(
      [&server](ready_event& event, const event_emitter&) {
        server.broadcast("notify.game.started", {
          {"component", event.component}
        });
      }
    )
  );

  // Bridge close_event -> notify.state.changed
  emitter.connect<close_event>(
    std::function<void(close_event&, const event_emitter&)>(
      [&server](close_event& event, const event_emitter&) {
        server.broadcast("notify.state.changed", {
          {"reason", "close"},
          {"detail", event.reason}
        });
      }
    )
  );
}

} // namespace rpc
