#include <IconsFontAwesome6.h>
#include <SFML/Graphics.hpp>
#include <fmt/format.h>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
using namespace entt::literals;
#include <game/specs/light.hpp>
#include <game/viewport.hpp>
#include <imgui-SFML.h>
#include <imgui-stl.hpp>
#include <imgui.h>
#include <imgui_entt_entity_editor.hpp>
#include <libcolor/libcolor.hpp>
#include <magic_enum.hpp>
#include <string>

std::string new_key_sprite;
std::string new_key_color;
std::string new_editor_name;
std::string new_key_prob;
std::string new_key_lt;
std::string new_key_lf;
bool show_unknown = true;
float new_prob = 0;

namespace MM {
template <>
void ComponentEditorWidget<hf::meta>(entt::registry &registry,
                                     entt::registry::entity_type e) {
  auto &m = registry.get<hf::meta>(e);
  ImGui::InputText("Name", m.name);
  ImGui::InputText("Description", m.description);
  ImGui::InputText("ID", m.id);
}

template <>
void ComponentEditorWidget<hf::tags>(entt::registry &registry,
                                     entt::registry::entity_type e) {

  auto &t = registry.get<hf::tags>(e);
  if (ImGui::ListEdit("Tags", t.tags)) {
    registry.emplace_or_replace<hf::tags>(e, t);
  }
}
template <>
void ComponentEditorWidget<hf::size>(entt::registry &registry,
                                     entt::registry::entity_type e) {
  auto &s = registry.get<hf::size>(e);
  auto &emitter = entt::locator<event_emitter>::value();
  if (ImGui::InputInt("Width", &s.width)) {
    // emitter.publish(redraw_event{});
  }
  if (ImGui::InputInt("Height", &s.height)) {
    // emitter.publish(redraw_event{});
  }
}

// template <>
// void ComponentEditorWidget<hf::room>(entt::registry &registry,
//                                      entt::registry::entity_type e) {
//   auto &r = registry.get<hf::room>(e);
//   ImGui::Text(
//       fmt::format("Type: {}", magic_enum::enum_name(r.room->type)).c_str());
// }

template <>
void ComponentEditorWidget<hf::children>(entt::registry &registry,
                                         entt::registry::entity_type e) {
  if (ImGui::TreeNode(fmt::format("childs##{}", (int)e).c_str(), "%s Entities",
                      ICON_FA_CUBE)) {
    auto ch = registry.get<hf::children>(e);
    for (auto child : ch.children) {
      // Editor::drawEntityInfo(child);
    }
    ImGui::TreePop();
  }
}
// template <>
// void ComponentEditorWidget<hf::cell>(entt::registry &registry,
//                                      entt::registry::entity_type e) {
//   auto &p = registry.get<hf::cell>(e);
//   auto &emitter = entt::locator<event_emitter>::value();
//   auto cell = p.cell;
//
//   auto cti = 0;
//   auto n = 0;
//   std::vector<std::string> ct_names;
//   for (auto ct : Cell::types) {
//     ct_names.push_back(ct.name);
//     if (cell != nullptr && ct == cell->type) {
//       cti = n;
//     }
//     n++;
//   }
//   if (ImGui::Combo("Type##cell_type", &cti, ct_names)) {
//     if (cell != nullptr) {
//       // auto &engine = entt::locator<DrawEngine>::value();
//       // mapUtils::updateCell(cell, Cell::types[cti], cell->tags.tags);
//       // engine.tilesCache.clear();
//       // emitter.publish(redraw_event{});
//     }
//   }
// }

// template <>
// void ComponentEditorWidget<hf::pickable>(entt::registry &registry,
//                                          entt::registry::entity_type e) {
//   auto &p = registry.get<hf::pickable>(e);
//   ImGui::InputText("Category", p.category.name);
//   ImGui::InputText("Unidentified name", p.unidName);
//   ImGui::InputInt("Count", &p.count);
//   ImGui::Checkbox("Identfied", &p.identified);
// }
// template <>
// void ComponentEditorWidget<hf::wearable>(entt::registry &registry,
//                                          entt::registry::entity_type e) {
//   auto &w = registry.get<hf::wearable>(e);
//
//   constexpr auto types = magic_enum::enum_values<WearableType>();
//   constexpr auto types_names = magic_enum::enum_names<WearableType>();
//   std::vector<std::string> names;
//   for (auto n : types_names) {
//     names.push_back(std::string(n));
//   }
//   auto wt = static_cast<int>(w.wearableType);
//   if (ImGui::Combo("Wearable type", &wt, names)) {
//     w.wearableType = types[wt];
//   }
//   ImGui::InputInt("Durability", &w.durability);
// }

template <>
void ComponentEditorWidget<hf::visible>(entt::registry &registry,
                                        entt::registry::entity_type e) {
  auto &v = registry.get<hf::visible>(e);
  ImGui::InputText("Type", v.type);
  ImGui::InputText("Sign", v.sign);
}
template <>
void ComponentEditorWidget<hf::glow>(entt::registry &registry,
                                     entt::registry::entity_type e) {
  auto &g = registry.get<hf::glow>(e);
  ImGui::InputFloat("Distance", &g.distance);
  constexpr auto types = magic_enum::enum_values<LightType>();
  constexpr auto types_names = magic_enum::enum_names<LightType>();
  std::vector<std::string> names;
  for (auto n : types_names) {
    names.push_back(std::string(n));
  }
  auto gt = static_cast<int>(g.type);
  if (ImGui::Combo("Light type", &gt, names)) {
    g.type = types[gt];
  }
  ImGui::InputInt("Brightness", &g.bright);
  ImGui::InputInt("Flick", &g.flick);
  ImGui::InputInt("Pulse", &g.pulse);
  ImGui::Checkbox("Passive", &g.passive);
}
template <>
void ComponentEditorWidget<hf::ineditor>(entt::registry &registry,
                                         entt::registry::entity_type e) {
  auto &ie = registry.get<hf::ineditor>(e);
  if (registry.all_of<hf::ineditor>(e)) {
    ie = registry.get<hf::ineditor>(e);
  } else {
    registry.emplace_or_replace<hf::ineditor>(e, hf::ineditor{});
    ie = registry.get<hf::ineditor>(e);
  }

  ImGui::BeginDisabled();
  ImGui::InputText("Name", ie.name);
  ImGui::EndDisabled();
  if (ImGui::Button(ICON_FA_FLOPPY_DISK)) {
    ie.name = new_editor_name;
  }
  ImGui::SameLine();
  ImGui::InputText("New Name", new_editor_name);
  if (ImGui::Checkbox("Selected", &ie.selected)) {
    registry.emplace_or_replace<hf::ineditor>(e, ie);
  }
  if (ie.folders.size() > 0) {
    std::string path = "";
    for (auto f : ie.folders) {
      path = fmt::format("{} > {}", path, f);
    }
    ImGui::BulletText("Folders: %s", path.c_str());
  }
  if (ImGui::ListEdit("Folders", ie.folders)) {
    registry.emplace_or_replace<hf::ineditor>(e, ie);
  }
  if (ie.icon != "") {
    ImGui::BulletText(fmt::format("Icon: {}", ie.icon).c_str());
  }
}
template <>
void ComponentEditorWidget<hf::position>(entt::registry &registry,
                                         entt::registry::entity_type e) {
  float GUI_SCALE = entt::monostate<"gui_scale"_hs>{};
  auto &p = registry.get<hf::position>(e);
  auto &emitter = entt::locator<event_emitter>::value();
  if (ImGui::Button(ICON_FA_ARROWS_TO_DOT)) {
    // emitter.publish(center_event{p.x, p.y});
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(90 * GUI_SCALE);
  if (ImGui::InputInt("x##Position", &p.x)) {
    registry.emplace_or_replace<hf::position>(e, p);
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(90 * GUI_SCALE);
  if (ImGui::InputInt("y##Position", &p.y)) {
    registry.emplace_or_replace<hf::position>(e, p);
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(90 * GUI_SCALE);
  if (ImGui::InputInt("z##Position", &p.z)) {
    registry.emplace_or_replace<hf::position>(e, p);
  }
}
template <>
void ComponentEditorWidget<hf::player>(entt::registry &registry,
                                       entt::registry::entity_type e) {}
template <>
void ComponentEditorWidget<hf::creature>(entt::registry &registry,
                                         entt::registry::entity_type e) {}
template <>
void ComponentEditorWidget<hf::script>(entt::registry &registry,
                                       entt::registry::entity_type e) {
  auto &c = registry.get<hf::script>(e);
  ImGui::InputText("path", c.path);
}
template <>
void ComponentEditorWidget<hf::obstacle>(entt::registry &registry,
                                         entt::registry::entity_type e) {
  auto &c = registry.get<hf::obstacle>(e);
  auto &emitter = entt::locator<event_emitter>::value();
  if (ImGui::Checkbox("passThrough", &c.passThrough)) {
    registry.emplace_or_replace<hf::obstacle>(e, c);
    // emitter.publish(redraw_event{});
  }
  if (ImGui::Checkbox("seeThrough", &c.seeThrough)) {
    registry.emplace_or_replace<hf::obstacle>(e, c);
    // emitter.publish(redraw_event{});
  }
  if (ImGui::Checkbox("Intercative", &c.interactive)) {
    registry.emplace_or_replace<hf::obstacle>(e, c);
    // emitter.publish(redraw_event{});
  }
  if (ImGui::InputInt("passAddCost", &c.passAddCost)) {
    // emitter.publish(redraw_event{});
  }
  if (ImGui::InputInt("interactionCost", &c.interactionCost)) {
    // emitter.publish(redraw_event{});
  }
}
template <>
void ComponentEditorWidget<hf::vision>(entt::registry &registry,
                                       entt::registry::entity_type e) {
  auto &s = registry.get<hf::vision>(e);
  auto &emitter = entt::locator<event_emitter>::value();
  if (ImGui::InputFloat("Distance", &s.distance)) {
    // emitter.publish(redraw_event{});
  }
}
template <>
void ComponentEditorWidget<hf::renderable>(entt::registry &registry,
                                           entt::registry::entity_type e) {
  float GUI_SCALE = entt::monostate<"gui_scale"_hs>{};
  auto viewport = entt::locator<Viewport>::value();
  auto r = hf::renderable{};
  if (registry.all_of<hf::renderable>(e)) {
    r = registry.get<hf::renderable>(e);
  } else {
    registry.emplace_or_replace<hf::renderable>(e, r);
    r = registry.get<hf::renderable>(e);
  }

  if (ImGui::Checkbox("Hidden", &r.hidden)) {
    registry.emplace_or_replace<hf::renderable>(e, r);
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(90 * GUI_SCALE);
  if (ImGui::InputInt("zIndex", &r.zIndex)) {
    registry.emplace_or_replace<hf::renderable>(e, r);
  }

  auto k = r.spriteKey;
  auto v = viewport.tileSet.sprites[k];
  sf::Sprite s;
  s.setTexture(*viewport.tilesTextures[v[0]]);
  s.setTextureRect(viewport.getTileRect(v[1], v[2]));
  ImGui::Image(s,
               sf::Vector2f(viewport.tileSet.size.first * GUI_SCALE,
                            viewport.tileSet.size.second * GUI_SCALE),
               sf::Color::White, sf::Color::Transparent);
  ImGui::SameLine();
  std::vector<const char *> _ts;
  int s_idx = 0;
  auto n = 0;
  std::transform(viewport.tileSet.sprites.begin(),
                 viewport.tileSet.sprites.end(), std::back_inserter(_ts),
                 [&](auto sk) {
                   if (sk.first == r.spriteKey) {
                     s_idx = n;
                   }
                   char *r = new char[sk.first.size() + 1];
                   std::strcpy(r, sk.first.c_str());
                   n++;
                   return r;
                 });
  if (ImGui::Combo("Sprite", &s_idx, _ts.data(), _ts.size())) {
    r.spriteKey = std::string(_ts[s_idx]);
    registry.emplace_or_replace<hf::renderable>(e, r);
  }

  ImGui::SetNextItemWidth(150);
  if (ImGui::InputText("Color", r.fgColor)) {
    registry.emplace_or_replace<hf::renderable>(e, r);
  }
  if (r.fgColor.length() > 1 && r.fgColor[0] == '#') {
    ImGui::SameLine();
    auto color = LibColor::Color::fromHexString(r.fgColor);
    float col[4] = {
        color.r / 255.f,
        color.g / 255.f,
        color.b / 255.f,
        color.a / 255.f,
    };
    if (ImGui::ColorEdit4(r.fgColor.c_str(), col,
                          ImGuiColorEditFlags_NoInputs |
                              ImGuiColorEditFlags_NoLabel |
                              ImGuiColorEditFlags_AlphaPreview |
                              ImGuiColorEditFlags_AlphaBar)) {
      auto c = LibColor::Color(col[0] * 255, col[1] * 255, col[2] * 255,
                               col[3] * 255);
      r.fgColor = c.hexA();
      registry.emplace_or_replace<hf::renderable>(e, r);
    }
  }

  if (ImGui::Checkbox("Has BG", &r.hasBg)) {
    registry.emplace_or_replace<hf::renderable>(e, r);
  }
  if (r.hasBg) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText("BG Color", r.bgColor)) {
      registry.emplace_or_replace<hf::renderable>(e, r);
    }
    if (r.bgColor.length() > 1 && r.bgColor[0] == '#') {
      ImGui::SameLine();
      auto color = LibColor::Color::fromHexString(r.bgColor);
      float bcol[4] = {
          color.r / 255.f,
          color.g / 255.f,
          color.b / 255.f,
          color.a / 255.f,
      };
      if (ImGui::ColorEdit4(r.bgColor.c_str(), bcol,
                            ImGuiColorEditFlags_NoInputs |
                                ImGuiColorEditFlags_NoLabel |
                                ImGuiColorEditFlags_AlphaPreview |
                                ImGuiColorEditFlags_AlphaBar)) {
        auto c = LibColor::Color(bcol[0] * 255, bcol[1] * 255, bcol[2] * 255,
                                 bcol[3] * 255);
        r.bgColor = c.hexA();
        registry.emplace_or_replace<hf::renderable>(e, r);
      }
    }
  }

  if (ImGui::Checkbox("has border", &r.hasBorder)) {
    registry.emplace_or_replace<hf::renderable>(e, r);
  }
  if (r.hasBorder) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText("Border Color", r.borderColor)) {
      registry.emplace_or_replace<hf::renderable>(e, r);
    }
    if (r.borderColor.length() > 1 && r.borderColor[0] == '#') {
      ImGui::SameLine();
      auto color = LibColor::Color::fromHexString(r.borderColor);
      float bcol[4] = {
          color.r / 255.f,
          color.g / 255.f,
          color.b / 255.f,
          color.a / 255.f,
      };
      if (ImGui::ColorEdit4(r.borderColor.c_str(), bcol,
                            ImGuiColorEditFlags_NoInputs |
                                ImGuiColorEditFlags_NoLabel |
                                ImGuiColorEditFlags_AlphaPreview |
                                ImGuiColorEditFlags_AlphaBar)) {
        auto c = LibColor::Color(bcol[0] * 255, bcol[1] * 255, bcol[2] * 255,
                                 bcol[3] * 255);
        r.borderColor = c.hexA();
        registry.emplace_or_replace<hf::renderable>(e, r);
      }
    }
  }

  std::vector<const char *> layers = {
      "cellsBg",     "cells",   "cellsBrd", "entitiesBg", "entities",
      "entitiesBrd", "heroBg",  "hero",     "heroBrd",    "light",
      "darkness",    "overlay", "debug"};
  int fgl_idx = std::distance(
      layers.begin(), std::find(layers.begin(), layers.end(), r.fgLayer));
  int bgl_idx = std::distance(
      layers.begin(), std::find(layers.begin(), layers.end(), r.bgLayer));
  int brl_idx = std::distance(
      layers.begin(), std::find(layers.begin(), layers.end(), r.brdLayer));
  if (ImGui::Combo("Layer", &fgl_idx, layers.data(), layers.size())) {
    r.fgLayer = std::string(layers[fgl_idx]);
    registry.emplace_or_replace<hf::renderable>(e, r);
  }
  if (ImGui::Combo("BG Layer", &bgl_idx, layers.data(), layers.size())) {
    r.bgLayer = std::string(layers[bgl_idx]);
    registry.emplace_or_replace<hf::renderable>(e, r);
  }
  if (ImGui::Combo("Border Layer", &brl_idx, layers.data(), layers.size())) {
    r.brdLayer = std::string(layers[brl_idx]);
    registry.emplace_or_replace<hf::renderable>(e, r);
  }
}

} // namespace MM
