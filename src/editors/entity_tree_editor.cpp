#include <IconsFontAwesome6.h>
#include <editors/entity_tree_editor.hpp>
#include <fmt/format.h>
#include <game_manager.hpp>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <utils/entt.hpp>

template <typename ContainerType, typename... T>
void EntityTreeEditor::render() {
  auto &gm = entt::locator<GameManager>::value();
  if (!gm.started) {
    return;
  }

  auto container = entt::locator<ContainerType>::value();

  ImGui::Begin(name.c_str());

  ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
  if (ImGui::BeginTabBar("StoresTabs", tab_bar_flags)) {
    for (auto data : container.stores) {
      if (ImGui::BeginTabItem(data->name.c_str())) {
        auto ents = data->registry.template view<T...>();
      }
    }
  }

  ImGui::End();
}
