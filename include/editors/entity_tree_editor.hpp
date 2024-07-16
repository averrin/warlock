#pragma once
#include <IconsFontAwesome6.h>
#include <fmt/format.h>
#include <game/game_manager.hpp>
#include <game/prototypes.hpp>
#include <game/state.hpp>
#include <imgui-stl.hpp>
#include <imgui.h>
#include <imgui_entt_entity_editor.hpp>
#include <map>
#include <memory>
#include <string>
#include <utils/entt.hpp>
#include <vector>

typedef std::vector<entt::entity> entity_list;
struct tree_node {
  entity_list entities;
  std::map<std::string, std::shared_ptr<tree_node>> children;
};

class EntityTreeEditor {
public:
  std::string name;
  EntityTreeEditor(std::string name);

  template <typename ContainerType, typename... T> void render() {
    auto &gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      return;
    }
    ImGui::Begin("Prototypes Editor");
    renderInline<ContainerType, T...>();
    ImGui::End();
  }

  template <typename ContainerType, typename... T> void renderInline() {
    auto &gm = entt::locator<GameManager>::value();
    auto &container = entt::locator<ContainerType>::value();

    if (ImGui::Button("Apply")) {
      // emitter.publish(regen_event{});
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
      // gm->loadData();
      // emitter.publish(resize_event{});
      // emitter.publish(regen_event{});
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
      gm.saveData();
    }
    ImGui::Separator();

    auto entityTree = std::make_shared<tree_node>();

    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("StoresTabs", tab_bar_flags)) {
      for (auto data : container.stores) {
        if (ImGui::BeginTabItem(data->name.c_str())) {
          auto ents = data->registry.template view<T...>();
          // auto ents = data->registry.view<entt::tag<"proto"_hs>>();
          for (auto e : ents) {
            if (!data->registry.template all_of<hf::ineditor>(e) ||
                data->registry.template get<hf::ineditor>(e).folders.size() ==
                    0 ||
                data->registry.template get<hf::ineditor>(e).folders.front() ==
                    "") {
              entityTree->entities.push_back(e);
            } else {
              auto ie = data->registry.template get<hf::ineditor>(e);
              if (ie.folders.size() == 0) {
                entityTree->entities.push_back(e);
              } else {
                auto current_node = entityTree;
                for (auto f : ie.folders) {
                  if (current_node->children.find(f) ==
                      current_node->children.end()) {
                    current_node->children[f] = std::make_shared<tree_node>();
                  }
                  current_node = current_node->children[f];
                }
                current_node->entities.push_back(e);
              }
            }
          }

          if (ImGui::TreeNode("ents", "%s Entities", ICON_FA_CUBE)) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Count: %lu", ents.size());
            ImGui::SameLine();
            if (ImGui::Button("New Entity")) {
              auto e = data->registry.create();
              data->registry.template emplace<entt::tag<"proto"_hs>>(e);
            }

            std::function<void(std::shared_ptr<tree_node> v)> it;
            it = [&](std::shared_ptr<tree_node> v) {
              for (auto e : v->entities) {
                drawEntityInfo(data->registry, e);
              }
              for (auto [fn, f] : v->children) {
                if (ImGui::TreeNode(
                        fmt::format("{} {}", ICON_FA_FOLDER, fn).c_str())) {
                  it(f);
                  ImGui::TreePop();
                }
              }
            };

            it(entityTree);

            ImGui::TreePop();
          }
          ImGui::EndTabItem();
        }
      }
      ImGui::EndTabBar();
    }
  }

  void drawEntityInfo(entt::registry &registry, entt::entity e);

  // std::map<std::string, std::string> cache = {};
  // std::map<std::string, float> cache_f = {};
};
