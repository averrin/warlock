#include <IconsFontAwesome6.h>
#include <editors/entity_tree_editor.hpp>
#include <fmt/format.h>
// #include <game/components.hpp>
#include <game/game_manager.hpp>
#include <game/prototypes.hpp>
#include <imgui-stl.hpp>
#include <imgui.h>
#include <imgui_entt_entity_editor.hpp>
#include <utils/entt.hpp>

EntityTreeEditor::EntityTreeEditor(std::string name) : name(name) {
  auto &entityEditor = entt::locator<MM::EntityEditor<entt::entity>>::emplace();
  entityEditor.registerComponent<hf::meta>("Meta");
  entityEditor.registerComponent<hf::position>("Position");
  entityEditor.registerComponent<hf::visible>("Visible");
  entityEditor.registerComponent<hf::renderable>("Renderable");
  entityEditor.registerComponent<hf::player>("Player");
  entityEditor.registerComponent<hf::creature>("Creature");
  entityEditor.registerComponent<hf::obstacle>("Obstacle");
  entityEditor.registerComponent<hf::script>("Script");
  entityEditor.registerComponent<hf::vision>("Vision");
  entityEditor.registerComponent<hf::ineditor>("Editor");
  entityEditor.registerComponent<hf::pickable>("Pickable");
  entityEditor.registerComponent<hf::wearable>("Wearable");
  entityEditor.registerComponent<hf::glow>("Glow");
  entityEditor.registerComponent<hf::cell>("Cell");
  entityEditor.registerComponent<hf::room>("Room");
  entityEditor.registerComponent<hf::children>("Children");
  entityEditor.registerComponent<hf::size>("Size");
  entityEditor.registerComponent<hf::wall>("Wall");
  entityEditor.registerComponent<hf::tags>("Tags");
  entityEditor.registerComponent<hf::overwrite>("Overwrite");
}

void EntityTreeEditor::drawEntityInfo(std::shared_ptr<RegistryStore> data,
                                      entt::entity e) {
  auto t = data->registry.all_of<hf::meta>(e)
               ? data->registry.get<hf::meta>(e).name.c_str()
               : "Entity";
  t = (data->registry.all_of<hf::ineditor>(e) &&
       data->registry.get<hf::ineditor>(e).name != "")
          ? data->registry.get<hf::ineditor>(e).name.c_str()
          : t;
  auto icon = (data->registry.all_of<hf::ineditor>(e) &&
               data->registry.get<hf::ineditor>(e).icon != "")
                  ? data->registry.get<hf::ineditor>(e).icon.c_str()
                  : ICON_FA_CUBE;
  auto selected = (data->registry.all_of<hf::ineditor>(e) &&
                   data->registry.get<hf::ineditor>(e).selected);
  auto col = selected ? (ImVec4)ImColor::HSV(1.f / 6.f, 0.86f, 1.0f)
                      : (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.99f);
  ImGui::PushStyleColor(ImGuiCol_Text, col);
  auto title = fmt::format("{} {}: {}", icon, t, (int)e);
  // Editor::drawEntityEditor();
  if (ImGui::TreeNode(title.c_str())) {
    ImGui::PopStyleColor(1);
    auto &entityEditor = entt::locator<MM::EntityEditor<entt::entity>>::value();
    entityEditor.renderEditor(data->registry, e);
    ImGui::TreePop();
  } else {
    ImGui::PopStyleColor(1);
  }
}

// template <typename ContainerType, typename... T>
void EntityTreeEditor::render() {
  auto &gm = entt::locator<GameManager>::value();
  if (!gm.started) {
    return;
  }

  // auto container = entt::locator<ContainerType>::value();
  auto &container = entt::locator<Prototypes>::value();

  // ImGui::Begin(name.c_str());
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
        // auto ents = data->registry.template view<T...>();
        auto ents = data->registry.view<entt::tag<"proto"_hs>>();
        for (auto e : ents) {
          if (!data->registry.all_of<hf::ineditor>(e) ||
              data->registry.get<hf::ineditor>(e).folders.size() == 0 ||
              data->registry.get<hf::ineditor>(e).folders.front() == "") {
            entityTree->entities.push_back(e);
          } else {
            auto ie = data->registry.get<hf::ineditor>(e);
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
            // data->registry.emplace<T...>(e);
          }

          std::function<void(std::shared_ptr<tree_node> v)> it;
          it = [&](std::shared_ptr<tree_node> v) {
            for (auto e : v->entities) {
              drawEntityInfo(data, e);
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

  ImGui::End();
}
