#include <editors/power_editor.hpp>
#include <game/systems/power.hpp>
#include <fmt/format.h>
#include <imgui.h>
#include <implot.h>
#include <utils/entt.hpp>
#include <effolkronium/random.hpp>
#include <ranges>
using Random = effolkronium::random_static;

void PowerEditor::render() {
  ImGui::Begin("Power info");
  if (!entt::locator<PowerInfo>::has_value()) {
    ImGui::Text("No power info available");
    ImGui::End();
    return;
  }
  auto &info = entt::locator<PowerInfo>::value();


  for (auto net : info.networks) {
    ImGui::Text(net.data.name.c_str());
    ImGui::Text("    Production: %f", net.production);
    ImGui::Text("    Consumption: %f", net.consumption);
    ImGui::Text("    Accumulated: %f", net.accumulated);
    ImGui::Text("    Accumulated available: %f", net.accumulated_available);
    ImGui::Text("    Battery count: %d", net.battery_count);

    ImGui::Text("    Net Production: %f", net.production - net.consumption);
    auto balance = net.production - net.consumption + net.accumulated_available;
    ImGui::Text("    Net Available: %f", balance);
    auto to_discharge =
        net.consumption - net.production > net.accumulated_available
            ? net.accumulated_available
            : net.consumption - net.production;
    /*
    if (net.battery_count > 0 && to_discharge > 0) {
      ImGui::Text("    Discharged %f", to_discharge);
      ImGui::Text("    Discharge per battery (predicted) %f",
                  to_discharge / net.battery_count);
    }
    if (balance < 0) {
      ImGui::Text("    Energy deficit");
    } else {
      ImGui::Text("    Energy balance");
    }
    */

    int history = 100;
    float x[history];
    for (int i = 0; i < history; ++i) {
      x[i] = i;
    }

    std::vector<float> combined_range = {};
    combined_range.insert(combined_range.end(), net.history["production"].begin(), net.history["production"].end());
    combined_range.insert(combined_range.end(), net.history["total"].begin(), net.history["total"].end());
    combined_range.insert(combined_range.end(), net.history["consumption"].begin(), net.history["consumption"].end());

    // Find the minimum value
    auto min_value = std::ranges::min(combined_range);

    // Find the maximum value
    auto max_value = std::ranges::max(combined_range);

    int vpad = 500;
    // ImGui::SliderFloat("History",&history,1,30,"%.1f s");
    // static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;
    static ImPlotAxisFlags flags;
    if (ImPlot::BeginPlot(fmt::format("Net stats##{}", net.data.name).c_str(),
                          ImVec2(0,0))) {
        // ImPlot::SetupAxes(nullptr, nullptr, flags, flags);
        ImPlot::SetupAxisLimits(ImAxis_X1,0, history);
        ImPlot::SetupAxisLimits(ImAxis_Y1,min_value - vpad,max_value + vpad);
        // ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL,0.5f);
        // ImPlot::PlotShaded("Mouse X", &sdata1.Data[0].x, &sdata1.Data[0].y, sdata1.Data.size(), -INFINITY, 0, sdata1.Offset, 2 * sizeof(float));
        ImPlot::PlotLine("Production", x, &net.history["production"][0], net.history["production"].size());
        ImPlot::PlotLine("Total", x, &net.history["total"][0], net.history["total"].size());
        // ImPlot::PlotShaded("Production", x, &net.history["total"][0], net.history["total"].size(), net.consumption);
        ImPlot::PlotLine("Consumption", x, &net.history["consumption"][0], net.history["consumption"].size());
        ImPlot::EndPlot();
    }

    ImGui::Separator();
  }

  ImGui::End();
}
