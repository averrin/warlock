#include <editors/power_editor.hpp>
#include <effolkronium/random.hpp>
#include <fmt/format.h>
#include <game/systems/power.hpp>
#include <imgui.h>
#include <implot.h>
#include <ranges>
#include <utils/entt.hpp>
using Random = effolkronium::random_static;

void PowerEditor::render() {
  ImGui::Begin("Power info");
  if (!entt::locator<PowerInfo>::has_value()) {
    ImGui::Text("No power info available");
    ImGui::End();
    return;
  }
  auto &info = entt::locator<PowerInfo>::value();

  if (info.networks.empty()) {
    ImGui::Text("No networks available");
    ImGui::End();
    return;
  }

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

    constexpr int history = 100;
    float x[history];
    for (int i = 0; i < history; ++i) {
      x[i] = static_cast<float>(i);
    }

    std::vector<float> combined_range = {};
    combined_range.insert(combined_range.end(),
                          net.history["production"].begin(),
                          net.history["production"].end());
    combined_range.insert(combined_range.end(), net.history["total"].begin(),
                          net.history["total"].end());
    combined_range.insert(combined_range.end(),
                          net.history["consumption"].begin(),
                          net.history["consumption"].end());

    // Find the minimum value
    auto min_value = std::ranges::min(combined_range);

    // Find the maximum value
    auto max_value = std::ranges::max(combined_range);

    int vpad = 500;
    static ImPlotAxisFlags flags;
    if (ImPlot::BeginPlot(fmt::format("Net stats##{}", net.data.name).c_str(),
                          ImVec2(0, 0))) {
      ImPlot::SetupAxisLimits(ImAxis_X1, 0, history);
      ImPlot::SetupAxisLimits(ImAxis_Y1, min_value - vpad, max_value + vpad);
      std::vector<float> prod_v(net.history["production"].begin(), net.history["production"].end());
      std::vector<float> total_v(net.history["total"].begin(), net.history["total"].end());
      std::vector<float> cons_v(net.history["consumption"].begin(), net.history["consumption"].end());
      ImPlot::PlotLine("Production", x, prod_v.data(),
                       static_cast<int>(prod_v.size()));
      ImPlot::PlotLine("Total", x, total_v.data(),
                       static_cast<int>(total_v.size()));
      ImPlot::PlotLine("Consumption", x, cons_v.data(),
                       static_cast<int>(cons_v.size()));
      ImPlot::EndPlot();
    }

    ImGui::Separator();
  }

  ImGui::End();
}
