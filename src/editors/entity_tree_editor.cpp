#include <editors/entity_tree_editor.hpp>
#include <game/components/frame.hpp>

EntityTreeEditor::EntityTreeEditor(std::string name) : name(name) {
  auto &entityEditor = entt::locator<MM::EntityEditor<entt::entity>>::emplace();
  entityEditor.registerComponent<hf::meta>("Meta");
  entityEditor.registerComponent<hf::position>("Position");
  entityEditor.registerComponent<hf::visible>("Visible");
  entityEditor.registerComponent<hf::renderable>("Renderable");
  entityEditor.registerComponent<hf::ineditor>("Editor");
  entityEditor.registerComponent<Frame>("Frame");
  entityEditor.registerComponent<Connection>("Connection");
  /*
  entityEditor.registerComponent<hf::player>("Player");
  entityEditor.registerComponent<hf::creature>("Creature");
  entityEditor.registerComponent<hf::obstacle>("Obstacle");
  entityEditor.registerComponent<hf::script>("Script");
  entityEditor.registerComponent<hf::vision>("Vision");
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
  */
}

void EntityTreeEditor::drawEntityInfo(entt::registry &registry,
                                      entt::entity e) {
  auto color = (ImVec4)ImColor::HSV(0.0f, 0.0f, 0.99f);
  auto override = registry.all_of<entt::tag<"override"_hs>>(e);
  if (override) {
    // color = (ImVec4)ImColor::HSV(210.f / 255.f, 70.f / 255.f, 100.f / 255.f);
  }
  auto t = registry.all_of<hf::meta>(e) ? registry.get<hf::meta>(e).name.c_str()
                                        : "Entity";
  t = (registry.all_of<hf::ineditor>(e) &&
       registry.get<hf::ineditor>(e).name != "")
          ? registry.get<hf::ineditor>(e).name.c_str()
          : t;
  auto icon = (registry.all_of<hf::ineditor>(e) &&
               registry.get<hf::ineditor>(e).icon != "")
                  ? registry.get<hf::ineditor>(e).icon.c_str()
                  : ICON_FA_CUBE;
  if (override) {
    icon = fmt::format("{}{}", icon, ICON_FA_CIRCLE).c_str();
  }
  auto selected = (registry.all_of<hf::ineditor>(e) &&
                   registry.get<hf::ineditor>(e).selected);
  if (selected) {
    color = (ImVec4)ImColor::HSV(1.f / 6.f, 0.86f, 1.0f);
  }
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  auto title = fmt::format("{} {}: {}", icon, t, (int)e);
  // Editor::drawEntityEditor();
  if (ImGui::TreeNode(title.c_str())) {
    ImGui::PopStyleColor(1);
    auto &entityEditor = entt::locator<MM::EntityEditor<entt::entity>>::value();
    entityEditor.renderEditor(registry, e);
    ImGui::TreePop();
  } else {
    ImGui::PopStyleColor(1);
  }
}
