#include <rpc/event_bridge.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <utils/entt.hpp>
#include <game/nexus_api.hpp>
#include <nlohmann/json.hpp>

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

  // Bridge NexusApi events
  emitter.connect<nexus_toast_event>(
    std::function<void(nexus_toast_event&, const event_emitter&)>(
      [&server](nexus_toast_event& event, const event_emitter&) {
        server.broadcast("nexus.toast", {
          {"message", event.message},
          {"type", event.type}
        });
      }
    )
  );

  emitter.connect<nexus_marker_event>(
    std::function<void(nexus_marker_event&, const event_emitter&)>(
      [&server](nexus_marker_event& event, const event_emitter&) {
        server.broadcast("nexus.markers", {
          {"action", "set"},
          {"marker", {
            {"x", event.x},
            {"y", event.y},
            {"label", event.label},
            {"color", event.color}
          }}
        });
      }
    )
  );

  emitter.connect<nexus_clear_markers_event>(
    std::function<void(nexus_clear_markers_event&, const event_emitter&)>(
      [&server](nexus_clear_markers_event& event, const event_emitter&) {
        server.broadcast("nexus.markers", {
          {"action", "clear"}
        });
      }
    )
  );

  emitter.connect<nexus_remove_marker_event>(
    std::function<void(nexus_remove_marker_event&, const event_emitter&)>(
      [&server](nexus_remove_marker_event& event, const event_emitter&) {
        server.broadcast("nexus.markers", {
          {"action", "remove"},
          {"label", event.label}
        });
      }
    )
  );

  emitter.connect<nexus_indicator_event>(
    std::function<void(nexus_indicator_event&, const event_emitter&)>(
      [&server](nexus_indicator_event& event, const event_emitter&) {
        server.broadcast("nexus.indicators", {
          {"key", event.key},
          {"label", event.label},
          {"value", event.value},
          {"color", event.color}
        });
      }
    )
  );

  emitter.connect<spendable_pool_changed_event>(
    std::function<void(spendable_pool_changed_event&, const event_emitter&)>(
      [&server](spendable_pool_changed_event& event, const event_emitter&) {
        nlohmann::json amounts = nlohmann::json::object();
        for (const auto& [k, v] : event.amounts) {
          amounts[k] = v;
        }
        server.broadcast("spendable.pool", {{"amounts", amounts}});
      }
    )
  );
}

} // namespace rpc
