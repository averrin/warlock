#include <IconsFontAwesome6.h>
#include <SFML/Graphics.hpp>
#include <editors/entity_tree_editor.hpp>
#include <editors/game_editor.hpp>
#include <effolkronium/random.hpp>
#include <fmt/format.h>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/systems/thermal.hpp>
#include <imgui-SFML.h>
#include <imgui-stl.hpp>
#include <imgui.h>
#include <imgui_entt_entity_editor.hpp>
#include <implot.h>
#include <misc/cpp/imgui_stdlib.h>
#include <ranges>
#include <utils/entt.hpp>
using Random = effolkronium::random_static;
#include <editors/helpers.hpp>

void GameEditor::render() {
  auto &gm = entt::locator<GameManager>::value();
  auto &current_state = entt::locator<State>::value();
  ImGui::Begin("Game");
  if (!gm.started ||
      current_state.registry.template storage<entt::entity>().size() == 0) {
    ImGui::Text("Game not started");
    ImGui::End();
    return;
  }

  ImGui::SetNextItemWidth(250);
  ImGui::InputText(fmt::format("##nf-name").c_str(), &cache["fn"]);
  ImGui::SameLine();
  if (ImGui::Button("New Frame")) {
    gm.addFrame(cache["fn"]);
  }
  ImGui::Separator();
  ImGui::SetNextItemWidth(90);
  ImGui::InputInt("Source", &cache_i["src"]);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(90);
  ImGui::InputInt("Dest", &cache_i["dst"]);
  ImGui::SameLine();
  // TODO: add combo box for Connection Type

  static int connection_type_idx = 0;
  const char *connection_types[] = {"POWER", "DATA"};
  ImGui::SetNextItemWidth(90);
  ImGui::Combo("Connection Type", &connection_type_idx, connection_types,
               IM_ARRAYSIZE(connection_types));

  if (ImGui::Button("New Connection")) {
    ConnectionType selected_type =
        static_cast<ConnectionType>(connection_type_idx);
    gm.addConnection(cache_i["src"], cache_i["dst"], selected_type);
  }
  ImGui::Separator();
  ImGui::Separator();

  auto env = current_state.registry.get<Environment>((entt::entity)0);
  ImGui::Text(
      fmt::format(fmt::runtime("Time: {:#02d}:{:#02d} Temperature: {:#02f}C"),
                  env.minutes / 60, env.minutes % 60, env.temperature)
          .c_str());
  ImGui::Text(fmt::format("Air Flow: {:#02f}", env.airFlow).c_str());
  ImGui::Text(fmt::format("Sun: {:#02f}", env.sun).c_str());
  ImGui::Text(fmt::format("Is Day: {}", env.isDay).c_str());

  int insp_id = entt::monostate<"inspected_frame"_hs>{};
  if (env.temperatures.find(-1) != env.temperatures.end() && insp_id != -1) {
    auto &entityEditor = entt::locator<MM::EntityEditor<entt::entity>>::value();
    auto e = entt::entity(insp_id);

    ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("InspectTabs", tab_bar_flags)) {
      if (ImGui::BeginTabItem("Systems")) {
        for (auto system : gm.systems) {
          ImGui::Checkbox(system->name.c_str(), &system->enabled);
          ImGui::SameLine();
          ImGui::SetNextItemWidth(120);
          ImGui::InputFloat(fmt::format("##{}", system->name).c_str(),
                            &system->targetInterval);
        }

        ImGui::Text("Frames:");
        ImGui::Indent();
        auto &current_state = entt::locator<State>::value();

        static ImGuiTableFlags flags =
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
            ImGuiTableFlags_ContextMenuInBody;
        if (ImGui::BeginTable("table1", 5, flags)) {
          for (auto &f : current_state.registry.view<Frame>()) {
            auto &frame = current_state.registry.get<Frame>(f);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Button(
                    fmt::format("{}##fact-{}", ICON_FA_PLAY, frame.data.id)
                        .c_str())) {
              frame.activate();
            }
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(
                fmt::format("{} [{}]", frame.data.name, frame.data.id).c_str());
            ImGui::TableSetColumnIndex(2);
            EntityTreeEditor::ComponentsPanel(frame);
            ImGui::TableSetColumnIndex(3);
            auto consumption = 0.0f;
            auto total_consumption = 0.0f;
            for (auto c : frame.components) {
              auto cons = c->data.get_or<float>("consumption", 0.0f);
              total_consumption += cons;
              if (c->state == ComponentState::ACTIVE) {
                consumption += cons;
              }
            }
            ImGui::Text(
                fmt::format("{:.1f}/{:.1f}", consumption, total_consumption)
                    .c_str());

            ImGui::TableSetColumnIndex(4);
            if (ImGui::Button(
                    fmt::format("View##bf-{}", frame.data.id).c_str())) {
              ImGui::OpenPopup(
                  fmt::format("{}##f-{}", frame.data.name, frame.data.id)
                      .c_str());
            }
          }
          ImGui::EndTable();
        }
        ImGui::Unindent();
        ImGui::Separator();

        if (ImGui::BeginTable("table2", 2, flags)) {
          for (auto &f : current_state.registry.view<Frame>()) {
            auto &frame = current_state.registry.get<Frame>(f);
            auto hasStorage = false;
            for (auto c : frame.components) {
              if (c->storage != nullptr) {
                hasStorage = true;
              }
            }
            if (!hasStorage) {
              continue;
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text(
                fmt::format("{} [{}]", frame.data.name, frame.data.id).c_str());
            ImGui::TableSetColumnIndex(1);
            for (auto c : frame.components) {
              if (c->storage != nullptr) {
                for (auto slot : c->storage->slots) {
                  if (slot.stack != nullptr) {
                    ImGui::Text(fmt::format("{}: {}", slot.stack->item.name,
                                            slot.stack->amount)
                                    .c_str());
                  }
                }
              }
            }
          }
          ImGui::EndTable();
        }

        ImGui::Separator();
        if (ImGui::Button("Activate all")) {
          for (auto &f : current_state.registry.view<Frame>()) {
            auto frame = current_state.registry.get<Frame>(f);
            frame.activate();
          }
        }
        ImGui::EndTabItem();
      }

      /*
      if (ImGui::BeginTabItem("Entity")) {
        entityEditor.renderEditor(current_state.registry, e);
        ImGui::EndTabItem();
      }
      */
      if (ImGui::BeginTabItem("Temperature")) {
        static ImPlotAxisFlags flags = ImPlotLegendFlags_Outside;

        if (ImPlot::BeginPlot(
                fmt::format("Temperatures##tp-{}", insp_id).c_str())) {

          ImPlot::SetupLegend(ImPlotLocation_East, flags);
          ImPlot::SetupAxisLimits(ImAxis_X1, 0, history);
          ImPlot::SetupAxisLimits(ImAxis_Y1, -200, 200);

          ImPlot::TagY(ThermalSystem::heat_effect_temp, ImVec4(0, 1, 1, 1),
                       "Overheat");
          ImPlot::PlotInfLines("Overheat", &ThermalSystem::heat_effect_temp, 1,
                               ImPlotInfLinesFlags_Horizontal);
          ImPlot::TagY(ThermalSystem::cold_effect_temp, ImVec4(0, 1, 1, 1),
                       "Freeze");
          ImPlot::PlotInfLines("Freeze", &ThermalSystem::cold_effect_temp, 1,
                               ImPlotInfLinesFlags_Horizontal);

          ImPlot::TagY(ThermalSystem::max_break_temp, ImVec4(0, 1, 1, 1),
                       "Fatal Heat");
          ImPlot::PlotInfLines("Fatal Heat", &ThermalSystem::max_break_temp, 1,
                               ImPlotInfLinesFlags_Horizontal);
          ImPlot::TagY(ThermalSystem::min_break_temp, ImVec4(0, 1, 1, 1),
                       "Fatal Cold");
          ImPlot::PlotInfLines("Fatal Cold", &ThermalSystem::min_break_temp, 1,
                               ImPlotInfLinesFlags_Horizontal);

          ImPlot::PlotLine("Environment", x, &env.temperatures[-1][0],
                           env.temperatures[-1].size());
          if (current_state.registry.all_of<Frame>(e)) {
            auto &frame = current_state.registry.get<Frame>(e);
            auto &comps = frame.components;
            for (auto &component : comps) {
              if (env.temperatures.find(component->data.id) ==
                      env.temperatures.end() ||
                  env.temperatures[component->data.id].size() == 0) {
                return;
              }
              ImPlot::PlotLine(fmt::format("{}", component->data.name).c_str(),
                               x, &env.temperatures[component->data.id][0],
                               env.temperatures[component->data.id].size());
            }
          }
          ImPlot::EndPlot();
        }
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Items")) {
        const auto &items = gm.items->loader->get_items();
        for (const auto &[name, item] : items) {
          if (ImGui::CollapsingHeader(name.c_str())) {
            ImGui::Indent();
            ImGui::Text("Name: %s", item.name.c_str());
            ImGui::Text("Description: %s", item.description.c_str());
            ImGui::Text("Stack: %d", item.stack);
            ImGui::Unindent();
          }
        }
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Recipes")) {
        const auto &recipes = gm.items->loader->get_recipes();
        for (const auto &recipe : recipes) {
          if (ImGui::CollapsingHeader(recipe.name.c_str())) {
            ImGui::Indent();
            ImGui::Text("Name: %s", recipe.name.c_str());
            ImGui::Text("Description: %s", recipe.description.c_str());
            ImGui::Text("Time Cost: %.2f", recipe.timeCost);
            ImGui::Text("Power Cost: %.2f", recipe.powerCost);

            ImGui::Text("Inputs:");
            for (const auto &input : recipe.inputs) {
              ImGui::BulletText("Item: %s, Amount: %d", input.item.name.c_str(),
                                input.amount);
            }

            ImGui::Text("Outputs:");
            for (const auto &output : recipe.outputs) {
              ImGui::BulletText("Item: %s, Amount: %d",
                                output.item.name.c_str(), output.amount);
            }

            ImGui::Text("Available on:");
            for (auto &item : recipe.availableOn) {
              ImGui::BulletText(item.c_str());
            }
            ImGui::Unindent();
          }
        }
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }

  auto viewed_frame = gm.input->viewed_frame;
  if (viewed_frame != -1) {
    auto &frame = current_state.registry.get<Frame>((entt::entity)viewed_frame);
    ImGui::OpenPopup(
        fmt::format("{}##f-{}", frame.data.name, frame.data.id).c_str());

    if (ImGui::BeginPopup(
            fmt::format("{}##f-{}", frame.data.name, frame.data.id).c_str())) {

      ImGui::Text(
          fmt::format("{} [{}]", frame.data.name, frame.data.id).c_str());
      MM::ComponentEditorWidget<Frame>(current_state.registry,
                                       (entt::entity)viewed_frame);
      ImGui::EndPopup();
    }
  } else if (gm.input->show_context_menu) {
    ImGui::OpenPopup("Context Menu");
  }
  if (ImGui::BeginPopup("Context Menu")) {
    if (ImGui::MenuItem("Create empty frame")) {
      gm.startFramePlacement();
    }
    ImGui::EndPopup();
  }

  ImGui::End();
}
