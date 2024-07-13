#include <IconsFontAwesome6.h>
#include <SFML/Graphics.hpp>
#include <editors/tileset_editor.hpp>
#include <filesystem>
#include <fmt/format.h>
#include <game/viewport.hpp>
#include <imgui-SFML.h>
#include <imgui-stl.hpp>
#include <imgui.h>
#include <libcolor/libcolor.hpp>
#include <misc/cpp/imgui_stdlib.h>
#include <utils/entt.hpp>
namespace fs = std::filesystem;

int ts_idx = 0;
std::vector<std::string> ts = {"boxy"};

void TilesetEditor::render() {
  auto &viewport = entt::locator<Viewport>::value();
  if (!viewport.started) {
    return;
  }
  fs::path PATH = entt::monostate<"path"_hs>{};
  // ImGui::Begin(name.c_str());
  ImGui::Begin("Tileset Editor");

  float GUI_SCALE = entt::monostate<"gui_scale"_hs>{};
  auto &emitter = entt::locator<event_emitter>::value();

  if (ImGui::Combo("Tileset", &ts_idx, ts)) {
    auto path = PATH / fs::path("tilesets") / ts[ts_idx];
    viewport.loadTileset(path);
    // emitter.publish(regen_event{});
  }
  ImGui::BulletText("Size: %dx%d; gap: %d\n", viewport.tileSet.size.first,
                    viewport.tileSet.size.second, viewport.tileSet.gap);
  ImGui::BulletText("Maps: %lu\n", viewport.tileSet.maps.size());

  if (ImGui::Button("Apply")) {
    // emitter.publish(regen_event{});
  }
  ImGui::SameLine();
  if (ImGui::Button("Reload")) {
    auto path = PATH / fs::path("tilesets") / ts[ts_idx];
    viewport.loadTileset(path);
    // emitter.publish(regen_event{});
  }
  ImGui::SameLine();
  if (ImGui::Button("Save")) {
    // saveTileset();
  }
  ImGui::Separator();

  viewport.colors.erase("");
  if (ImGui::TreeNode("Colors")) {
    for (auto &el : viewport.colors.items()) {
      if (el.key() == "VARIATIONS")
        continue;
      if (el.key() == "WANDERING")
        continue;
      if (ImGui::TreeNode(el.key().c_str())) {
        std::vector<std::string> to_remove;
        for (auto &e : el.value().items()) {
          auto color = LibColor::Color::fromHexString(e.value());
          float col[4] = {
              color.r / 255.f,
              color.g / 255.f,
              color.b / 255.f,
              color.a / 255.f,
          };
          ImGui::Button(e.key().c_str());
          ImGui::SameLine(220);

          ImGui::SetNextItemWidth(150);
          auto cs = e.value().get<std::string>();
          if (ImGui::InputText(fmt::format("##{}", e.key()).c_str(), cs)) {
            viewport.colors[el.key()][e.key()] = cs;
            // emitter.publish(regen_event{});
          }
          ImGui::SameLine();
          if (ImGui::ColorEdit4(e.key().c_str(), col,
                                ImGuiColorEditFlags_NoInputs |
                                    ImGuiColorEditFlags_NoLabel |
                                    ImGuiColorEditFlags_AlphaPreview |
                                    ImGuiColorEditFlags_AlphaBar)) {
            auto c = LibColor::Color(col[0] * 255, col[1] * 255, col[2] * 255,
                                     col[3] * 255);
            viewport.colors[el.key()][e.key()] = c.hexA();
            // emitter.publish(regen_event{});
          }
          ImGui::SameLine();
          if (ImGui::Button(
                  fmt::format("{}##{}", ICON_FA_TRASH, e.key()).c_str())) {
            to_remove.push_back(e.key());
          }
        }
        for (auto rk : to_remove) {
          viewport.colors[el.key()].erase(rk);
        }
        if (to_remove.size() > 0) {
          // emitter.publish(regen_event{});
        }
        // auto new_key =
        // fmt::format("NEW_COLOR_{}", viewport.colors[el.key()].size());
        ImGui::InputText("key", &cache["new_key_color"]);
        ImGui::SameLine();
        if (ImGui::Button(fmt::format("Add##{}", el.key()).c_str())) {
          viewport.colors[el.key()][cache["new_key_color"]] = "#eeeeeeff";
        }
        ImGui::TreePop();
      }
    }
    ImGui::TreePop();
  }

  viewport.tileSet.sprites.erase("");
  if (ImGui::TreeNode("Sprites")) {
    std::vector<std::string> to_remove;
    for (auto [k, v] : viewport.tileSet.sprites) {
      sf::Sprite s;
      s.setTexture(*viewport.tilesTextures[v[0]]);
      s.setTextureRect(viewport.getTileRect(v[1], v[2]));
      // s.setOrigin(viewport.tileSet.size.first / 2,
      //             viewport.tileSet.size.second / 2);
      // s.setRotation(90 * v[3]);
      ImGui::Image(s,
                   sf::Vector2f(viewport.tileSet.size.first,
                                viewport.tileSet.size.second) *
                       GUI_SCALE,
                   sf::Color::White, sf::Color::Transparent);
      ImGui::SameLine();
      ImGui::Button(k.c_str());
      ImGui::SameLine(220);
      ImGui::SetNextItemWidth(80);
      if (ImGui::InputInt(fmt::format("##{}{}", k, 0).c_str(),
                          &(viewport.tileSet.sprites[k][0]))) {
        // engine.tilesCache.clear();
        // engine.invalidate();
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(80);
      if (ImGui::InputInt(fmt::format("##{}{}", k, 1).c_str(),
                          &(viewport.tileSet.sprites[k][1]))) {
        // engine.tilesCache.clear();
        // engine.invalidate();
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(80);
      if (ImGui::InputInt(fmt::format("##{}{}", k, 2).c_str(),
                          &(viewport.tileSet.sprites[k][2]))) {
        // engine.tilesCache.clear();
        // engine.invalidate();
      }
      // ImGui::SameLine();
      // ImGui::SetNextItemWidth(80);
      // if (ImGui::InputInt(fmt::format("##{}{}", k, 3).c_str(),
      //                     &(viewport.tileSet.sprites[k][3]))) {
      //   engine.tilesCache.clear();
      //   engine.invalidate();
      // }

      ImGui::SameLine();
      if (ImGui::Button(fmt::format("{}##{}", ICON_FA_TRASH, k).c_str())) {
        to_remove.push_back(k);
      }
    }
    for (auto rk : to_remove) {
      viewport.tileSet.sprites.erase(rk);
    }
    if (to_remove.size() > 0) {
      // emitter.publish(regen_event{});
    }

    // auto new_key =
    //     fmt::format("NEW_SPRITE_{}", viewport.tileSet.sprites.size());
    ImGui::InputText("key", &cache["new_key_sprite"]);
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
      viewport.tileSet.sprites[cache["new_key_sprite"]] = {0, 0, 0};
    }
    ImGui::TreePop();
  }
  if (ImGui::TreeNode("Sprite Variations")) {
    for (auto [k, _] : viewport.tileSet.spriteVariations) {
      if (ImGui::CollapsingHeader(k.c_str())) {
        for (auto v : viewport.tileSet.spriteVariations[k]) {
          ImGui::Indent();
          ImGui::Text(v.c_str());
          ImGui::Unindent();
        }
        // TODO: add/del variation
      }
      // TODO: add/del sprite
    }
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Color Variations")) {
    for (auto [k, _] : viewport.tileSet.colorVariations) {
      if (ImGui::CollapsingHeader(k.c_str())) {
        for (auto v : viewport.tileSet.colorVariations[k]) {
          ImGui::Indent();
          ImGui::Text(v.c_str());
          ImGui::Unindent();
        }
        // TODO: add/del variation
      }
      // TODO: add/del sprite
    }
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Color Wandering")) {
    for (auto [k, _] : viewport.tileSet.colorWandering) {
      if (ImGui::CollapsingHeader(k.c_str())) {
        for (auto [v, p] : viewport.tileSet.colorWandering[k]) {
          ImGui::Indent();
          ImGui::Text(v.c_str());
          ImGui::SameLine();
          ImGui::SetNextItemWidth(90 * GUI_SCALE);
          ImGui::InputFloat(fmt::format("##{}{}0", k, v).c_str(),
                            &(viewport.tileSet.colorWandering[k][v][0]));
          ImGui::SameLine();
          ImGui::SetNextItemWidth(90 * GUI_SCALE);
          ImGui::InputFloat(fmt::format("##{}{}1", k, v).c_str(),
                            &(viewport.tileSet.colorWandering[k][v][1]));
          ImGui::Unindent();
        }
        // TODO: add/del variation
      }
      // TODO: add/del sprite
    }
    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Preview")) {
    auto n = 0;
    for (auto t : viewport.tilesTextures) {
      auto size = t->getSize();
      sf::Sprite s;
      s.setTexture(*viewport.tilesTextures[n]);
      ImGui::Text("%s", viewport.tileSet.maps[n].c_str());
      ImGui::Image(s, sf::Vector2f(size.x, size.y), sf::Color::White,
                   sf::Color::Transparent);
      ImGui::Text("\n");
      n++;
    }
    ImGui::TreePop();
  }

  ImGui::End();
}
