#include <IconsFontAwesome6.h>
#include <editors/meta_data_editor.hpp>
#include <fmt/format.h>
#include <game_manager.hpp>
#include <imgui.h>
#include <meta_data.hpp>
#include <misc/cpp/imgui_stdlib.h>
#include <mutex>
#include <utils/entt.hpp>

void MetaDataEditor::render() {
  auto &gm = entt::locator<GameManager>::value();
  if (!gm.started) {
    return;
  }
  auto &metaData = entt::locator<MetaData>::value();
  ImGui::Begin(name.c_str());

  if (ImGui::Button("Apply")) {
    // emitter.publish(regen_event{});
  }
  ImGui::SameLine();
  if (ImGui::Button("Reload")) {
    gm.loadData();
    ImGui::End();
    return;
    // emitter.publish(resize_event{});
    // emitter.publish(regen_event{});
  }
  ImGui::SameLine();
  if (ImGui::Button("Save")) {
    gm.saveData();
  }

  //TODO: add a way to add new stores

  ImGui::Separator();

  ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
  if (ImGui::BeginTabBar("StoresTabs", tab_bar_flags)) {
    for (auto data : metaData.stores) {
      if (ImGui::BeginTabItem(data->name.c_str())) {

        if (ImGui::CollapsingHeader("Location Features")) {
          ImGui::Indent();
          for (auto &[k, _] : data->mapFeatures) {
            if (ImGui::CollapsingHeader(k.c_str())) {
              ImGui::Indent();
              for (auto &[f, _] : data->mapFeatures[k]) {
                ImGui::SetNextItemWidth(80);
                ImGui::InputFloat(f.c_str(), &data->mapFeatures[k][f]);
                ImGui::SameLine();
                if (ImGui::Button(fmt::format(ICON_FA_XMARK"##Del feat {} {}", k, f).c_str())) {
                  data->mapFeatures[k].erase(f);
                  break;
                }
              }

              ImGui::SetNextItemWidth(80);
              ImGui::InputFloat(fmt::format("##New prob lf {}", k).c_str(),
                                &cache_f["prob"]);
              ImGui::SameLine();
              ImGui::SetNextItemWidth(160);
              ImGui::InputText(fmt::format("##New name lf {}", k).c_str(),
                               &cache["lf"]);
              ImGui::SameLine();
              if (ImGui::Button(fmt::format("Add##{}", k).c_str())) {
                data->mapFeatures[k][cache["lf"]] = cache_f["prob"];
              }
              ImGui::Unindent();
            }
          }
          ImGui::SetNextItemWidth(160);
          ImGui::InputText("##New name lt", &cache["lt"]);
          ImGui::SameLine();
          if (ImGui::Button("Add")) {
            data->mapFeatures[cache["lt"]] = {};
          }
          ImGui::Unindent();
        }
        if (ImGui::CollapsingHeader("Probabilities")) {
          ImGui::Indent();
          for (auto &[k, _] : data->probability) {
            ImGui::Indent();
            ImGui::SetNextItemWidth(80);
            ImGui::InputFloat(k.c_str(), &data->probability[k]);
            ImGui::SameLine();
            if (ImGui::Button(fmt::format(ICON_FA_XMARK"##Del prob {}", k).c_str())) {
              data->probability.erase(data->probability.find(k));
            }
            ImGui::Unindent();
          }
          ImGui::Indent();
          ImGui::SetNextItemWidth(80);
          ImGui::InputFloat("##New prob", &cache_f["prob"]);
          ImGui::SameLine();
          ImGui::SetNextItemWidth(160);
          ImGui::InputText("##New name", &cache["key"]);
          ImGui::SameLine();
          if (ImGui::Button("Add")) {
            data->probability[cache["key"]] = cache_f["prob"];
          }
          ImGui::Unindent();
          ImGui::Unindent();
        }

        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }

  ImGui::End();
}
