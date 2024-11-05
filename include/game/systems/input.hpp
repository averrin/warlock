#pragma once
#include <app/scene.hpp>
#include <deque>
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <game/system.hpp>
#include <utils/entt.hpp>
#include <utils/entt_draw.hpp>
#include <utils/entt_lua.hpp>
#include <vector>

struct input_wait_position {
  std::string proto_name = "";
};
struct input_suggests_position {
  wl::position position;
};
struct input_selected_position {
  wl::position position;
};

class InputSystem : public System {
  entt::entity getChildByName(entt::registry &registry, entt::entity parent,
                              const std::string &name) {
    auto &relation =
        registry.get_or_emplace<wl::relation>(parent, wl::relation{});
    for (auto &child : relation.children) {
      if (!registry.all_of<hf::meta>(child)) {
        continue;
      }
      auto &meta = registry.get<hf::meta>(child);
      if (meta.name == name) {
        return child;
      }
    }
    return entt::null;
  }

  bool positionIsOverFrame(wl::position pos) {
    auto &current_state = entt::locator<State>::value();
    auto frames_view = current_state.registry.view<Frame>();
    for (auto &f : frames_view) {
      auto hitbox = getChildByName(current_state.registry, f, "Frame Hitbox");
      if (hitbox == entt::null) {
        continue;
      }
      if (containsInHitbox(hitbox, pos)) {
        return true;
      }
    }
    return false;
  }

  bool containsInHitbox(entt::entity hitbox, wl::position pos) {
    auto &current_state = entt::locator<State>::value();
    auto transform = current_state.registry.get<wl::transform>(hitbox);
    auto rect = current_state.registry.get<wl::rect>(hitbox);

    if (transform.relative) {
      auto parent = current_state.registry.get<wl::transform>(
          current_state.registry.get<wl::relation>(hitbox).parent);
      transform.position.x = transform.position.x + parent.position.x;
      transform.position.y = transform.position.y + parent.position.y;
    }

    if (pos.x >= transform.position.x &&
        pos.x < transform.position.x + rect.width &&
        pos.y >= transform.position.y &&
        pos.y < transform.position.y + rect.height) {
      return true;
    }
    return false;
  }

public:
  void fixedUpdate() override;
  wl::position mouse_position;
  wl::position selected_position;
  wl::position suggested_position;

  bool wait_for_selection = false;
  int snap = 0;

  std::vector<int> selected_frames;
  int viewed_frame = -1;
  bool show_context_menu = false;

  void startPositionSelection(std::string proto_name, int s = 0) {
    auto &emitter = entt::locator<event_emitter>::value();
    snap = s;
    wait_for_selection = true;
    emitter.publish(input_wait_position{proto_name});
  }

  InputSystem() : System(15, "Input") {
    auto &current_state = entt::locator<State>::value();
    auto &emitter = entt::locator<event_emitter>::value();
    emitter.connect<sf_event_event>([&](auto &e, auto &em) {
      if (e.event.type == sf::Event::MouseButtonPressed) {
        auto frames_view = current_state.registry.view<Frame>();
        if (e.event.mouseButton.button == sf::Mouse::Button::Right) {
          viewed_frame = -1;
          selected_frames.clear();
          show_context_menu = false;

          if (wait_for_selection) {
            wait_for_selection = false;
            return;
          }

          auto onFrame = false;
          for (auto &f : frames_view) {
            auto sprite =
                getChildByName(current_state.registry, f, "Frame Sprite");
            auto &visual_state =
                current_state.registry.get_or_emplace<wl::visual_state>(sprite);
            visual_state.selected = false;
            current_state.registry.replace<wl::visual_state>(sprite,
                                                             visual_state);
            if (visual_state.hovered) {
              viewed_frame = (int)f;
              onFrame = true;
              break;
            }

            if (!onFrame) {
              show_context_menu = true;
            }
          }
        } else if (e.event.mouseButton.button == sf::Mouse::Button::Left) {

          if (wait_for_selection) {
            selected_position = suggested_position;
            wait_for_selection = false;
            emitter.publish(input_selected_position{selected_position});
            return;
          }

          show_context_menu = false;
          viewed_frame = -1;
          for (auto &f : frames_view) {
            auto sprite =
                getChildByName(current_state.registry, f, "Frame Sprite");
            auto &visual_state =
                current_state.registry.get_or_emplace<wl::visual_state>(sprite);
            if (visual_state.hovered) {
              visual_state.selected = !visual_state.selected;
              current_state.registry.replace<wl::visual_state>(sprite,
                                                               visual_state);
              return;
            }
          }
        }
      } else if (e.event.type == sf::Event::MouseMoved) {
        mouse_position = wl::position{(float)e.event.mouseMove.x,
                                      (float)e.event.mouseMove.y};

        if (wait_for_selection) {
          auto prev_position = suggested_position;
          suggested_position = mouse_position;
          if (snap != 0) {
            suggested_position.x =
                (std::round(suggested_position.x / snap) - 1) * snap;
            suggested_position.y =
                (std::round(suggested_position.y / snap) - 1) * snap;
          }
          if (positionIsOverFrame(suggested_position)) {
            suggested_position = prev_position;
          }
          emitter.publish(input_suggests_position{suggested_position});
          return;
        }

        auto frames_view = current_state.registry.view<Frame>();

        for (auto &f : frames_view) {
          auto &frame = current_state.registry.get<Frame>(f);
          auto &transform = current_state.registry.get<wl::transform>(f);
          auto hitbox =
              getChildByName(current_state.registry, f, "Frame Hitbox");
          if (hitbox == entt::null) {
            continue;
          }
          auto sprite =
              getChildByName(current_state.registry, f, "Frame Sprite");
          auto &visual_state =
              current_state.registry.get_or_emplace<wl::visual_state>(sprite);
          if (containsInHitbox(hitbox, mouse_position)) {
            visual_state.hovered = true;
            current_state.registry.replace<wl::visual_state>(sprite,
                                                             visual_state);
          } else {
            if (visual_state.hovered == true) {
              visual_state.hovered = false;
              current_state.registry.replace<wl::visual_state>(sprite,
                                                               visual_state);
            }
          }
          if (visual_state.selected) {
            selected_frames.push_back(frame.data.id);
          }
        }
      }
    });
  }
};
