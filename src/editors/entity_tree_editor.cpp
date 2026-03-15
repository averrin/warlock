#include <editors/entity_tree_editor.hpp>
#include <game/components/frame.hpp>
#include <utils/entt_draw.hpp>
#include <utils/entt_lua.hpp>

EntityTreeEditor::EntityTreeEditor(std::string name) : name(name) {
  auto &entityEditor = entt::locator<MM::EntityEditor<entt::entity>>::emplace();
  entityEditor.registerComponent<hf::meta>("Meta");
  entityEditor.registerComponent<hf::position>("Position");
  entityEditor.registerComponent<hf::visible>("Visible");
  entityEditor.registerComponent<hf::renderable>("Renderable");
  entityEditor.registerComponent<hf::ineditor>("Editor");
  entityEditor.registerComponent<Frame>("Frame");
  entityEditor.registerComponent<Connection>("Connection");
  entityEditor.registerComponent<Environment>("Environment");
  entityEditor.registerComponent<wl::transform>("Transform");
  entityEditor.registerComponent<wl::line>("Line");
  entityEditor.registerComponent<wl::text>("Text");
  entityEditor.registerComponent<wl::sprite>("Sprite");
  entityEditor.registerComponent<wl::visual_state>("State");
  entityEditor.registerComponent<wl::rect>("Rect");
}

void EntityTreeEditor::drawEntityInfo(entt::registry &registry,
                                      entt::entity e) {
  auto title = fmt::format("Details##en-details{}", (int)e);
  if (ImGui::TreeNode(title.c_str())) {
    auto &entityEditor = entt::locator<MM::EntityEditor<entt::entity>>::value();
    entityEditor.renderEditor(registry, e);
    ImGui::TreePop();
  }
}
