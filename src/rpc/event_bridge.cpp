#include <rpc/event_bridge.hpp>
#include <rpc/handlers/handler_utils.hpp>
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

  // Bridge component_state_changed -> web.log
  emitter.connect<component_state_changed>(
    std::function<void(component_state_changed&, const event_emitter&)>(
      [&server](component_state_changed& event, const event_emitter&) {
        logWebAction(server, "component.state", event.reason, {
          {"frame_id", event.frame_id},
          {"component_id", event.component_id},
          {"component_name", event.component_name},
          {"prev_state", event.prev_state},
          {"new_state", event.new_state}
        });
      }
    )
  );
}

} // namespace rpc
