#include <editors/power_editor.hpp>
#include <game/systems/power.hpp>
#include <imgui.h>
#include <utils/entt.hpp>

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
    ImGui::Text("    Production %f", net.production);
    ImGui::Text("    Consumption %f", net.consumption);
    ImGui::Text("    Accumulated %f", net.accumulated);
    ImGui::Text("    Accumulated available %f", net.accumulated_available);
    ImGui::Text("    Battery count %f", net.battery_count);

    ImGui::Text("    Net Production %f", net.production - net.consumption);
    auto balance = net.production - net.consumption + net.accumulated_available;
    ImGui::Text("    Net Available %f", balance);
    auto to_discharge =
        net.consumption - net.production > net.accumulated_available
            ? net.accumulated_available
            : net.consumption - net.production;
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
    history.push_back(net.production);

    ImGui::PlotLines("Production", history.data(), history.size());

    ImGui::Separator();
  }

  ImGui::End();
}
