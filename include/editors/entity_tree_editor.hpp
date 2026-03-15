#pragma once
#include <IconsFontAwesome6.h>
#include <fmt/format.h>
#include <game/game_manager.hpp>
#include <game/specs/light.hpp>
#include <imgui-SFML.h>
#include <imgui-stl.hpp>
#include <imgui.h>
#include <imgui_entt_entity_editor.hpp>
#include <game/components/frame.hpp>
#include <utils/entt_draw.hpp>

// Forward declarations of template specializations to prevent
// duplicate instantiations across translation units (MSVC LNK2005)
template <> void MM::ComponentEditorWidget<hf::meta>(entt::registry &, entt::entity);
template <> void MM::ComponentEditorWidget<hf::position>(entt::registry &, entt::entity);
template <> void MM::ComponentEditorWidget<hf::visible>(entt::registry &, entt::entity);
template <> void MM::ComponentEditorWidget<hf::renderable>(entt::registry &, entt::entity);
template <> void MM::ComponentEditorWidget<hf::ineditor>(entt::registry &, entt::entity);
template <> void MM::ComponentEditorWidget<Frame>(entt::registry &, entt::entity);
template <> void MM::ComponentEditorWidget<Connection>(entt::registry &, entt::entity);
template <> void MM::ComponentEditorWidget<Environment>(entt::registry &, entt::entity);
template <> void MM::ComponentEditorWidget<wl::transform>(entt::registry &, entt::entity);
template <> void MM::ComponentEditorWidget<wl::text>(entt::registry &, entt::entity);
template <> void MM::ComponentEditorWidget<wl::sprite>(entt::registry &, entt::entity);
template <> void MM::ComponentEditorWidget<wl::visual_state>(entt::registry &, entt::entity);
template <> void MM::ComponentEditorWidget<wl::rect>(entt::registry &, entt::entity);

#include <map>
#include <memory>
#include <string>
#include <utils/assets_loader.hpp>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <vector>

typedef std::vector<entt::entity> entity_list;
struct tree_node {
  entity_list entities;
  std::map<entt::entity, std::shared_ptr<tree_node>> children;
};

class EntityTreeEditor {
public:
  std::string name;
  EntityTreeEditor(std::string name);

  template <typename ContainerType, typename... T>
  void render(ContainerType &container) {
    auto &gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      return;
    }
    ImGui::Begin("Prototypes Editor");

    if (ImGui::Button("Apply")) {
      // emitter.publish(regen_event{});
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
      // gm->loadData();
      // emitter.publish(resize_event{});
      // emitter.publish(regen_event{});
    }
    if (ImGui::Button("Clear")) {
      container.clear();
      gm.saveData();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
      gm.saveData();
    }
    ImGui::Separator();

    renderInline<ContainerType, T...>(container);
    ImGui::End();
  }

  template <typename ContainerType, typename... T>
  void renderInline(ContainerType &container) {
    auto &gm = entt::locator<GameManager>::value();
    // auto &container = entt::locator<ContainerType>::value();

    auto entityTree = std::make_shared<tree_node>();

    std::map<std::string, entt::registry *> regs;
    regs["Root"] = &container.registry;
    for (auto data : container.stores) {
      regs[data->name] = &data->registry;
    }

    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("StoresTabs", tab_bar_flags)) {
      for (auto [name, reg] : regs) {
        if (ImGui::BeginTabItem(name.c_str())) {
          auto ents = reg->template view<T...>();
          /*
          // auto ents = data->registry.view<entt::tag<"proto"_hs>>();
          for (auto e : ents) {
            if (!reg->template all_of<hf::ineditor>(e) ||
                reg->template get<hf::ineditor>(e).folders.size() == 0 ||
                reg->template get<hf::ineditor>(e).folders.front() == "") {
              entityTree->entities.push_back(e);
            } else {
              auto ie = reg->template get<hf::ineditor>(e);
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

            if (reg->template all_of<wl::relation>(e)) {
              auto relation = reg->template get<wl::relation>(e);
              for (auto child : relation.children) {
                entityTree->entities.push_back(child);
              }
            }
          }

          std::vector<entt::entity> visited;
          for (auto e : ents) {
            if (std::find(visited.begin(), visited.end(), e) != visited.end()) {
              continue;
              visited.push_back(e);
              auto current_node = entityTree;
              current_node->entities.push_back(e);
              if (reg->template all_of<wl::relation>(e)) {
                auto relation = reg->template get<wl::relation>(e);
                for (auto child : relation.children) {
                  if (std::find(visited.begin(), visited.end(), child) ==
                      visited.end()) {
                    visited.push_back(child);
                    current_node->children[child] =
                        std::make_shared<tree_node>();
                    current_node->children[child]->entities.push_back(child);
                  }
                }
              }
            }
          }
            */

          if (ImGui::TreeNode("ents", "%s Entities", ICON_FA_CUBE)) {
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Count: %lu", ents.size());
            ImGui::Text("Count ALL: %lu",
                        reg->template storage<entt::entity>().size());
            ImGui::SameLine();
            if (ImGui::Button("New Entity")) {
              auto e = reg->create();
              reg->template emplace<T...>(e);
            }

            for (auto e : ents) {
              if (reg->template all_of<wl::relation>(e) &&
                  reg->template get<wl::relation>(e).parent != entt::null) {
                continue;
              }
              renderNode(reg, e);
            }

            ImGui::TreePop();
          }

          ImGui::EndTabItem();
        }
      }
      ImGui::EndTabBar();
    }
  }

  void renderNode(entt::registry *reg, entt::entity entity) {
    auto isize = 16;
    auto rel =
        reg->get_or_emplace<wl::relation>(entity, wl::relation{{}, entt::null});
    // auto meta = registry.get_or_emplace<hf::meta>(entity, hf::meta{});
    auto meta = reg->get_or_emplace<hf::meta>(entity, hf::meta{});
    ImGuiTreeNodeFlags nodeFlags =
        ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;

    float GUI_SCALE = entt::monostate<"gui_scale"_hs>{};
    sf::Texture t;
    sf::Sprite s;
    auto assetLoader = entt::locator<AssetLoader>::value();
    auto ts = assetLoader.getTextures();
    std::string icon = "cube.png";
    if (reg->all_of<hf::ineditor>(entity)) {
      auto ie = reg->get<hf::ineditor>(entity);
      if (ts.find(ie.icon) != ts.end()) {
        icon = ie.icon;
      }
    }
    s.setTexture(*ts[icon]);

    if (ImGui::TreeNodeEx((void *)(intptr_t)entity, nodeFlags, "")) {
      ImGui::SameLine();
      ImGui::Image(s, sf::Vector2f(isize * GUI_SCALE, isize * GUI_SCALE),
                   sf::Color::White, sf::Color::Transparent);
      ImGui::SameLine();
      ImGui::Text(fmt::format("{}: {}", meta.name, (int)entity).c_str());
      ImGui::SameLine();
      if (ImGui::Button(
              fmt::format("{}##{}", ICON_FA_ARROW_TURN_DOWN, (int)entity)
                  .c_str())) {
        auto child = reg->create();
        reg->emplace<wl::relation>(child, wl::relation{{}, entity});
        reg->emplace<hf::meta>(child, hf::meta{"Child", "Child", ""});
        auto &rel = reg->get_or_emplace<wl::relation>(
            entity, wl::relation{{}, entt::null});
        rel.children.push_back(child);
      }

      drawEntityInfo(*reg, entity);

      for (auto child : rel.children) {
        renderNode(reg, child);
      }
      ImGui::TreePop();
    } else {
      ImGui::SameLine();
      ImGui::Image(s, sf::Vector2f(isize * GUI_SCALE, isize * GUI_SCALE),
                   sf::Color::White, sf::Color::Transparent);
      ImGui::SameLine();
      ImGui::Text(fmt::format("{}: {}", meta.name, (int)entity).c_str());
    }
  }

  static void drawEntityInfo(entt::registry &registry, entt::entity e);

  static bool ComponentEditor(Frame &f, std::shared_ptr<Component> c);
  static void MetaDataEditor(Metadata &meta);

  static void ComponentsPanel(Frame &frame);
  // std::map<std::string, std::string> cache = {};
  // std::map<std::string, float> cache_f = {};
};
