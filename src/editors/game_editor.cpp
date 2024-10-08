#include <editors/game_editor.hpp>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui-stl.hpp>
#include <misc/cpp/imgui_stdlib.h>
#include <implot.h>
#include <utils/entt.hpp>
#include <effolkronium/random.hpp>
#include <ranges>
#include <game/game_manager.hpp>
using Random = effolkronium::random_static;

void GameEditor::render() {
  auto &gm = entt::locator<GameManager>::value();
  ImGui::Begin("Game");
  if (!gm.started) {
    ImGui::Text("Game not started");
    ImGui::End();
    return;
  }

  ImGui::SetNextItemWidth(250);
  ImGui::InputText(fmt::format("##nf-name").c_str(),
                               &cache["fn"]);
  ImGui::SameLine();
  if(ImGui::Button("New Frame")) {
    gm.addFrame(cache["fn"]);
  }
  ImGui::Separator();
  ImGui::SetNextItemWidth(90);
  ImGui::InputInt("Source", &cache_i["src"]);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(90);
  ImGui::InputInt("Dest", &cache_i["dst"]);
  ImGui::SameLine();
  if(ImGui::Button("New Connection")) {
    gm.addConnection(cache_i["src"], cache_i["dst"]);
  }
  ImGui::Separator();
  ImGui::End();
}
