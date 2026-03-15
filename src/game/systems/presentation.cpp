#include <fmt/core.h>
#include <fmt/ranges.h>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <game/systems/code_execution.hpp>
#include <game/systems/presentation.hpp>
#include <liblog/liblog.hpp>
#include <ranges> // For ranges
#include <utils/entt_tools.hpp>

void PresentationSystem::resetDrawables() {
  fmt::print("Resetting drawables\n");
  auto &current_state = entt::locator<State>::value();

  std::map<int, entt::entity> frames;

  for (auto &f : current_state.registry.view<Frame>()) {
    auto &frame = current_state.registry.get<Frame>(f);
    auto frame_transform = current_state.registry.get<wl::transform>(f);
    frames[frame.data.id] = f;
    if (!current_state.registry.all_of<wl::relation>(f)) {
      current_state.registry.emplace_or_replace<wl::relation>(f);
    }
    auto rel = current_state.registry.get<wl::relation>(f);

    if (rel.children.size() > 0) {
      for (auto &child : rel.children) {
        if (current_state.registry.valid(child)) {
          current_state.registry.destroy(child);
        }
      }
      rel.children.clear();
    }
    current_state.registry.emplace_or_replace<wl::visual_state>(f);

    auto &text = makeEntity<wl::text>(current_state.registry, "Frame Text", f);
    auto text_entity = getChildByName(current_state.registry, "Frame Text", f);

    text.content = fmt::format("{} [{}]", frame.data.name, frame.data.id);
    text.color = sf::Color(100, 100, 255, 255);
    text.size = 20;

    auto &rect =
        makeEntity<wl::rect>(current_state.registry, "Frame Hitbox", f);
    auto rect_entity =
        getChildByName(current_state.registry, "Frame Hitbox", f);
    current_state.registry.emplace_or_replace<wl::transform>(
        rect_entity, wl::transform{
                         .relative = true,
                         .position = {0, 0},
                         .scale = 1,
                         .rotation = 0,
                         .layer = "hitboxes",
                     });
    ;
    rect.width = 128;
    rect.height = 128;
    wl::transform tt = {
        .relative = true,
        .position =
            {
                .x = 0,
                .y = rect.height,
            },
        .scale = 1,
        .rotation = 0,
        .layer = "frames",
    };
    current_state.registry.emplace_or_replace<wl::transform>(text_entity, tt);

    auto &sprite =
        makeEntity<wl::sprite>(current_state.registry, "Frame Sprite", f);
    auto sprite_entity =
        getChildByName(current_state.registry, "Frame Sprite", f);
    sprite.key = "d6.png";
    sprite.rect = rect;
    current_state.registry.emplace_or_replace<wl::transform>(
        sprite_entity, wl::transform{
                           .relative = true,
                           .position = {0, 0},
                           .scale = 1,
                           .rotation = 0,
                           .layer = "frames",
                       });
  }

  for (auto &c : current_state.registry.view<Connection>()) {
    auto &connection = current_state.registry.get<Connection>(c);
    auto source_frame = frames[connection.source];
    auto target_frame = frames[connection.target];

    auto source_hb =
        getChildByName(current_state.registry, "Frame Hitbox", source_frame);
    auto target_hb =
        getChildByName(current_state.registry, "Frame Hitbox", target_frame);

    auto source_transform =
        current_state.registry.get<wl::transform>(source_frame);
    auto target_transform =
        current_state.registry.get<wl::transform>(target_frame);
    auto source_rect = current_state.registry.get<wl::rect>(source_hb);
    auto target_rect = current_state.registry.get<wl::rect>(target_hb);

    wl::line line;

    auto x1 = source_transform.position.x + source_rect.width / 2;
    auto y1 = source_transform.position.y + source_rect.height / 2;
    auto x2 = target_transform.position.x + target_rect.width / 2;
    auto y2 = target_transform.position.y + target_rect.height / 2;
    line.layer = "connections";
    line.position1 = {x1, y1};
    line.position2 = {x2, y2};
    line.color = connection.type == ConnectionType::POWER ? sf::Color::Yellow
                                                          : sf::Color::Blue;
    line.thickness = 3;
    current_state.registry.emplace_or_replace<wl::line>(c, line);
  }
}

void PresentationSystem::fixedUpdate() {
  auto &current_state = entt::locator<State>::value();

  auto needs_redraw = false;
  frame_observer.each([&](const auto entity) { needs_redraw = true; });

  connection_observer.each([&](const auto entity) { needs_redraw = true; });
  if (!needs_redraw)
    return;
  resetDrawables();
}

PresentationSystem::PresentationSystem() : System(15, "Presentation") {
  auto &current_state = entt::locator<State>::value();
  frame_observer.connect(current_state.registry,
                         entt::collector.update<Frame>());
  connection_observer.connect(current_state.registry,
                              entt::collector.update<Connection>());
  auto &emitter = entt::locator<event_emitter>::value();

  emitter.connect<input_wait_position>([&](auto &e, auto &em) {
    if (position_markers.find(e.proto_name) != position_markers.end()) {
      position_marker = position_markers[e.proto_name];
    } else {
      position_marker = EnttTools::createEntityFromPrototype(
          e.proto_name, current_state.registry);
      position_markers[e.proto_name] = position_marker;
    }
    current_state.registry.emplace_or_replace<wl::visual_state>(
        position_marker, wl::visual_state{.ghost = true, .hidden = false});
  });

  emitter.connect<input_suggests_position>([&](auto &e, auto &em) {
    auto transform = current_state.registry.emplace_or_replace<wl::transform>(
        position_marker, wl::transform{
                             .relative = false,
                             .position = e.position,
                             .scale = 1,
                             .rotation = 0,
                             .layer = "markers",
                         });
  });
  emitter.connect<input_selected_position>([&](auto &e, auto &em) {
    current_state.registry.emplace_or_replace<wl::visual_state>(
        position_marker, wl::visual_state{.ghost = true, .hidden = true});
  });
}
