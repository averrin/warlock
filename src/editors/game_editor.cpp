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
#include <game/components/frame.hpp>
#include <game/state.hpp>
#include <imgui_entt_entity_editor.hpp>
using Random = effolkronium::random_static;

void GameEditor::render() {
  auto &gm = entt::locator<GameManager>::value();
  ImGui::Begin("Game");
  if (!gm.started) {
    ImGui::Text("Game not started");
    ImGui::End();
    return;
  }

  Environment *env;
  auto &current_state = entt::locator<State>::value();
  for (auto &e : current_state.registry.view<Environment>()) {
    env = &current_state.registry.get<Environment>(e);
    ImGui::Text(fmt::format(
      fmt::runtime("Time: {:#02d}:{:#02d} Temperature: {:#02f}C"),
        env->minutes / 60, env->minutes % 60,
        env->temperature).c_str());
    break;
  }

  int insp_id = entt::monostate<"inspected_frame"_hs>{};
  if (env->temperatures.find(-1) != env->temperatures.end() && insp_id != -1) {
    auto &entityEditor = entt::locator<MM::EntityEditor<entt::entity>>::value();
    auto e = entt::entity(insp_id);

    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("InspectTabs", tab_bar_flags))
    {
        if (ImGui::BeginTabItem("Entity"))
        {
            entityEditor.renderEditor(current_state.registry, e);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Temperature"))
        {
          static ImPlotAxisFlags flags;
          if (ImPlot::BeginPlot(fmt::format("Temperatures##tp-{}", insp_id).c_str(), ImVec2(0,0))) {
            ImPlot::SetupAxisLimits(ImAxis_X1,0, history);
            ImPlot::SetupAxisLimits(ImAxis_Y1,-200, 200);
            ImPlot::PlotLine("Environment", x,
                             &env->temperatures[-1][0], env->temperatures[-1].size());
            if(current_state.registry.all_of<Frame>(e)) {
              auto &frame = current_state.registry.get<Frame>(e);
              for(auto &component : frame.components) {
                if (env->temperatures.find(component->data.id) == env->temperatures.end() || env->temperatures[component->data.id].size() == 0) {
                return;
              }
                  ImPlot::PlotLine(fmt::format("{}", component->data.name).c_str(),
                                   x, &env->temperatures[component->data.id][0],
                                   env->temperatures[component->data.id].size());
              }
            }
            ImPlot::EndPlot();
          }
          ImGui::EndTabItem();
        }
      ImGui::EndTabBar();
    }
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
