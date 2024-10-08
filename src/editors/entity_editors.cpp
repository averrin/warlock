#include <IconsFontAwesome6.h>
#include <SFML/Graphics.hpp>
#include <fmt/format.h>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <game/game_manager.hpp>
using namespace entt::literals;
#include <game/components/frame.hpp>
#include <game/specs/light.hpp>
#include <game/viewport.hpp>
#include <imgui-SFML.h>
#include <imgui-stl.hpp>
#include <imgui.h>
#include <imgui_entt_entity_editor.hpp>
#include <libcolor/libcolor.hpp>
#include <magic_enum.hpp>
#include <string>
#include <TextEditor.h>

std::string new_key_sprite;
std::string new_key_color;
std::string new_editor_name;
std::string new_key_prob;
std::string new_key_lt;
std::string new_key_lf;
bool show_unknown = true;
float new_prob = 0;

const char* AttributeEasingTypeToString(AttributeEasingType type) {
    switch (type) {
        case AttributeEasingType::NONE: return "None";
        case AttributeEasingType::JITTER: return "Jitter";
        case AttributeEasingType::SAW: return "Saw";
        case AttributeEasingType::SIN: return "Sin";
        case AttributeEasingType::RANDOM_STEP: return "Random Step";
        default: return "Unknown";
    }
}

const char* ComponentMaterialToString(ComponentMaterial size) {
  switch (size) {
    case ComponentMaterial::ALUMINIUM: return "Aluminium";
    case ComponentMaterial::COPPER: return "Copper";
    case ComponentMaterial::STEEL: return "Steel";
    case ComponentMaterial::TITANIUM: return "Titanium";
    case ComponentMaterial::PLASTIC: return "Plastic";
    case ComponentMaterial::GLASS: return "Glass";
    default: return "Unknown";
  }
}

const char* ComponentSizeToString(ComponentSize size) {
    switch (size) {
        case ComponentSize::S: return "Small";
        case ComponentSize::M: return "Medium";
        case ComponentSize::L: return "Large";
        default: return "Unknown";
    }
}

const char* FrameSizeToString(FrameSize size) {
    switch (size) {
        case FrameSize::S: return "Small";
        case FrameSize::M: return "Medium";
        case FrameSize::L: return "Large";
        case FrameSize::G: return "Giant";
        default: return "Unknown";
    }
}

const char* ComponentStateToString(ComponentState state) {
  switch (state) {
    case ComponentState::DEACTIVATED: return "Deactivated";
    case ComponentState::ACTIVATING: return "Activating";
    case ComponentState::ACTIVE: return "Active";
    case ComponentState::DEACTIVATING: return "Deactivating";
    case ComponentState::ERROR: return "Error";
    case ComponentState::DESTROYED: return "Destroyed";
    case ComponentState::BLOCKED: return "Blocked";
    default: return "Unknown";
  }
}

const char* ComponentStateToIcon(ComponentState state) {
  switch (state) {
    case ComponentState::DEACTIVATED: return ICON_FA_STOP;
    case ComponentState::ACTIVATING: return ICON_FA_GEARS;
    case ComponentState::ACTIVE: return ICON_FA_CHECK;
    case ComponentState::DEACTIVATING: return ICON_FA_GEARS;
    case ComponentState::ERROR: return ICON_FA_TRIANGLE_EXCLAMATION;
    case ComponentState::DESTROYED: return ICON_FA_SKULL_CROSSBONES;
    case ComponentState::BLOCKED: return ICON_FA_BAN;
    default: return ICON_FA_QUESTION;
  }
}

void MetaDataEditor(Metadata &meta) {
  float GUI_SCALE = entt::monostate<"gui_scale"_hs>{};
  ImGui::SetNextItemWidth(90 * GUI_SCALE);
  ImGui::InputInt(fmt::format("ID##n-mde-{}", meta.id).c_str(), &meta.id);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(250 * GUI_SCALE);
  ImGui::InputText(fmt::format("Name##n-mde-{}", meta.id).c_str(), meta.name);
  ImGui::InputText(fmt::format("Description##d-mde-{}", meta.id).c_str(), meta.description);

  static std::map<int, std::shared_ptr<TextEditor>> editors;

  static const std::array<AttributeEasingType, 5> easing_types = {
      AttributeEasingType::NONE,
      AttributeEasingType::JITTER,
      AttributeEasingType::SAW,
      AttributeEasingType::SIN,
      AttributeEasingType::RANDOM_STEP
  };
  ImGui::Text("Attributes:");
  ImGui::Indent();
  for (auto [key, attr] : meta.attributes) {
    if(key == "code") {
      if (ImGui::CollapsingHeader(fmt::format("Code##code-{}", meta.id).c_str())) {
        ImGui::Indent();
        std::string v = attr->getBase<std::string>();
        if (editors.find(meta.id) == editors.end()) {
          editors[meta.id] = std::make_shared<TextEditor>();
          editors[meta.id]->SetLanguageDefinition(TextEditor::LanguageDefinitionId::Lua);
          editors[meta.id]->SetText(v);
        }

        if (ImGui::Button(ICON_FA_FLOPPY_DISK)) {
          attr->SetBaseValue(editors[meta.id]->GetText());
        }
        editors[meta.id]->Render(fmt::format("Code##{}-{}", key, meta.id).c_str());
      }
      continue;
    }

    ImGui::Text("[%s]: %s", key.c_str(), attr->ToString().c_str());
    if(attr->GetType() == AttributeType::FLOAT) {
      ImGui::SameLine();
      auto v = attr->getBase<float>();
      if(ImGui::InputFloat(fmt::format("##{}-{}", key, meta.id).c_str(), &v)) {
        attr->SetBaseValue(v);
      }

      bool value_changed = false;

      ImGui::Indent();
      if (ImGui::CollapsingHeader(fmt::format("Easing: {}##easing-{}-{}", AttributeEasingTypeToString(attr->easing.easing_type), key, meta.id).c_str())) {
          ImGui::Indent();
          int current_type = static_cast<int>(attr->easing.easing_type);
          if (ImGui::Combo("Easing Type", &current_type, [](void* data, int idx, const char** out_text) {
              *out_text = AttributeEasingTypeToString(static_cast<AttributeEasingType>(idx));
              return true;
          }, nullptr, easing_types.size())) {
              attr->easing.easing_type = static_cast<AttributeEasingType>(current_type);
              value_changed = true;
          }
          if (ImGui::SliderFloat("Easing Range", &attr->easing.easing_range, 0.0f, 1.0f, "%.3f")) {
              value_changed = true;
          }
          if (ImGui::InputFloat("Easing Period", &attr->easing.easing_period, 10.0f, 100.0f, "%.1f")) {
              attr->easing.easing_period = std::max(attr->easing.easing_period, 0.0f); // Ensure non-negative
              value_changed = true;
          }
      }
      ImGui::Unindent();
      if (value_changed) {
        attr->SetEasing(attr->easing);
      }
    } else if(attr->GetType() == AttributeType::INT) {
      ImGui::SameLine();
      auto v = attr->getBase<int>();
      if(ImGui::InputInt(fmt::format("##{}-{}", key, meta.id).c_str(), &v)) {
        attr->SetBaseValue(v);
      }
    } else if(attr->GetType() == AttributeType::STRING) {
      ImGui::SameLine();
      std::string v = attr->getBase<std::string>();
      if(ImGui::InputText(fmt::format("##{}-{}", key, meta.id).c_str(), v)) {
        attr->SetBaseValue(v);
      }
    } else if(attr->GetType() == AttributeType::BOOL) {
      ImGui::SameLine();
      auto v = attr->getBase<bool>();
      if(ImGui::Checkbox(fmt::format("##{}-{}", key, meta.id).c_str(), &v)) {
        attr->SetBaseValue(v);
      }
    }
  }
}

bool ComponentEditor(Frame &f, std::shared_ptr<Component> c) {
  static const std::array<ComponentSize, 3> component_sizes = {
    ComponentSize::S,
    ComponentSize::M,
    ComponentSize::L
  };

  static const std::array<ComponentState, 7> component_states = {
    ComponentState::DEACTIVATED,
    ComponentState::ACTIVATING,
    ComponentState::ACTIVE,
    ComponentState::DEACTIVATING,
    ComponentState::ERROR,
    ComponentState::DESTROYED,
    ComponentState::BLOCKED
  };

      if(ImGui::Button(fmt::format("{}##act-{}", ICON_FA_PLAY, c->data.id).c_str())) {
        c->activate();
      }
      ImGui::SameLine();
      if(ImGui::Button(fmt::format("{}##deact-{}", ICON_FA_STOP, c->data.id).c_str())) {
        c->deactivate();
      }
      ImGui::SameLine();
      ImGui::Text(ComponentStateToIcon(c->state));
      ImGui::SameLine();

      if(ImGui::Button(fmt::format("{}##del-{}", ICON_FA_TRASH, c->data.id).c_str())) {
        //use gm mutex
        auto &gm = entt::locator<GameManager>::value();

        gm.updateMutex.lock();
        f.components.erase(std::remove_if(f.components.begin(), f.components.end(), [&](auto &comp) {
          return comp->data.id == c->data.id;
        }), f.components.end());
        gm.updateMutex.unlock();
        return true;
      }
      ImGui::SameLine();
      if (ImGui::CollapsingHeader(
              fmt::format("{}", c->data.name.c_str())
                  .c_str())) {
        ImGui::Indent();
        if (c->error != "") {
          ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", c->error.c_str());
        }

        float GUI_SCALE = entt::monostate<"gui_scale"_hs>{};
        int current_state = static_cast<int>(c->state);
        ImGui::SetNextItemWidth(180 * GUI_SCALE);
        if (ImGui::Combo(fmt::format("State##st-{}", c->data.id).c_str(), &current_state, [](void* data, int idx, const char** out_text) {
            *out_text = ComponentStateToString(static_cast<ComponentState>(idx));
            return true;
        }, nullptr, component_states.size())) {
            c->state = static_cast<ComponentState>(current_state);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90 * GUI_SCALE);
        int current_size = static_cast<int>(c->size);
        if (ImGui::Combo(fmt::format("Size##sz-{}", c->data.id).c_str(), &current_size, [](void* data, int idx, const char** out_text) {
            *out_text = ComponentSizeToString(static_cast<ComponentSize>(idx));
            return true;
        }, nullptr, component_sizes.size())) {
            c->size = static_cast<ComponentSize>(current_size);
        }

  static const std::array<ComponentMaterial, 6> materials = {
    ComponentMaterial::ALUMINIUM,
    ComponentMaterial::COPPER,
    ComponentMaterial::STEEL,
    ComponentMaterial::TITANIUM,
    ComponentMaterial::PLASTIC,
    ComponentMaterial::GLASS
  };


  ImGui::SameLine();
  ImGui::SetNextItemWidth(120 * GUI_SCALE);
  int current_material = static_cast<int>(c->material);
  if (ImGui::Combo(fmt::format("Material##mat-{}", c->data.id).c_str(), &current_material, [](void* data, int idx, const char** out_text) {
      *out_text = ComponentMaterialToString(static_cast<ComponentMaterial>(idx));
      return true;
  }, nullptr, materials.size())) {
      c->material = static_cast<ComponentMaterial>(current_material);
  }

        MetaDataEditor(c->data);
        ImGui::Separator();
        ImGui::Unindent();

      }
  return false;
}

namespace MM {
template <>
void ComponentEditorWidget<Frame>(entt::registry &registry,
                                  entt::registry::entity_type e) {
  auto &f = registry.get<Frame>(e);

  static const std::array<FrameSize, 4> frame_sizes = {
    FrameSize::S,
    FrameSize::M,
    FrameSize::L,
    FrameSize::G
  };

  float GUI_SCALE = entt::monostate<"gui_scale"_hs>{};
  static const std::array<ComponentMaterial, 6> materials = {
    ComponentMaterial::ALUMINIUM,
    ComponentMaterial::COPPER,
    ComponentMaterial::STEEL,
    ComponentMaterial::TITANIUM,
    ComponentMaterial::PLASTIC,
    ComponentMaterial::GLASS
  };


  ImGui::SetNextItemWidth(120 * GUI_SCALE);
  int current_material = static_cast<int>(f.material);
  if (ImGui::Combo(fmt::format("Material##f-mat-{}", f.data.id).c_str(), &current_material, [](void* data, int idx, const char** out_text) {
      *out_text = ComponentMaterialToString(static_cast<ComponentMaterial>(idx));
      return true;
  }, nullptr, materials.size())) {
      f.material = static_cast<ComponentMaterial>(current_material);
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(90 * GUI_SCALE);
  int current_size = static_cast<int>(f.size);
  if (ImGui::Combo(fmt::format("Size##f-sz-{}", f.data.id).c_str(), &current_size, [](void* data, int idx, const char** out_text) {
      *out_text = FrameSizeToString(static_cast<FrameSize>(idx));
      return true;
  }, nullptr, frame_sizes.size())) {
      f.size = static_cast<FrameSize>(current_size);
  }
  if (ImGui::CollapsingHeader(
            fmt::format("Data##data-{}", f.data.id)
                .c_str())) {
    MetaDataEditor(f.data);
  }

  if(ImGui::Button(fmt::format("{}##fact-{}", ICON_FA_PLAY, f.data.id).c_str())) {
    f.activate();
  }
  ImGui::SameLine();
  if(ImGui::Button(fmt::format("{}##fdeact-{}", ICON_FA_STOP, f.data.id).c_str())) {
    f.deactivate();
  }
  ImGui::SameLine();
  if (ImGui::CollapsingHeader(
            fmt::format("Components##comp-{}", f.data.id)
                .c_str())) {
    ImGui::Indent();

    if (ImGui::Button("+ Add Component")) {
      ImGui::OpenPopup("Add Component");
    }

    if (ImGui::BeginPopup("Add Component")) {
      ImGui::TextUnformatted("Available:");
      ImGui::Separator();

      auto &gm = entt::locator<GameManager>::value();
      auto &emitter = entt::locator<event_emitter>::value();
      for (auto &comp_name : gm.components) {
        ImGui::PushID(comp_name.c_str());
        if (ImGui::Selectable(comp_name.c_str())) {
          emitter.publish(add_component{f, comp_name});
        }
        ImGui::PopID();
      }
      ImGui::EndPopup();
    }

    for (auto &c : f.components) {
      if(ComponentEditor(f, c)) {
        return;
      }
    }
  }
}

template <>
void ComponentEditorWidget<Connection>(entt::registry &registry,
                                       entt::registry::entity_type e) {
  auto &c = registry.get<Connection>(e);
  MetaDataEditor(c.data);

  ImGui::InputInt("Source", &c.source);
  ImGui::InputInt("Target", &c.target);
}

template <>
void ComponentEditorWidget<Environment>(entt::registry &registry,
                                       entt::registry::entity_type e) {
  auto &env = registry.get<Environment>(e);

  ImGui::InputInt("Time (minutes)", &env.minutes);
  ImGui::InputFloat("Temperature", &env.temperature);
  ImGui::InputFloat("Air Flow", &env.airFlow);
}

template <>
void ComponentEditorWidget<hf::meta>(entt::registry &registry,
                                     entt::registry::entity_type e) {
  auto &m = registry.get<hf::meta>(e);
  ImGui::InputText("Name", m.name);
  ImGui::InputText("Description##ce", m.description);
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
