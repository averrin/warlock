#include <rpc/dto.hpp>
#include <game/systems/items.hpp>
#include <magic_enum.hpp>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace rpc {

nlohmann::json serializeAttributeValue(const AttributeValue& val) {
  return std::visit([](auto&& v) -> nlohmann::json { return v; }, val);
}

nlohmann::json serializeAttribute(const std::string& key, const Attribute& attr) {
  nlohmann::json mods = nlohmann::json::array();
  for (const auto& m : attr.modifiers) {
    if (m) mods.push_back(m->name);
  }
  return {
    {"title", attr.GetTitle()},
    {"type", std::string(magic_enum::enum_name(attr.GetType()))},
    {"base_value", serializeAttributeValue(attr.GetBaseValue())},
    {"final_value", serializeAttributeValue(attr.GetFinalValue())},
    {"modifiers", mods}
  };
}

nlohmann::json serializeMetadata(const Metadata& data) {
  nlohmann::json attrs = nlohmann::json::object();
  for (const auto& [k, v] : data.attributes) {
    if (v) {
      attrs[k] = serializeAttribute(k, *v);
    }
  }
  return {
    {"id", data.id},
    {"name", data.name},
    {"description", data.description},
    {"icon", data.icon},
    {"attributes", attrs}
  };
}

nlohmann::json serializeComponent(const Component& comp) {
  // type = the string value of the "type" attribute, or name as fallback
  std::string type_str = comp.data.name;
  auto it = comp.data.attributes.find("type");
  if (it != comp.data.attributes.end() && it->second) {
    auto& attr = *it->second;
    if (attr.GetType() == AttributeType::STRING) {
      type_str = std::get<std::string>(attr.GetFinalValue());
    }
  }

  nlohmann::json attrs = nlohmann::json::object();
  for (const auto& [k, v] : comp.data.attributes) {
    if (v) {
      attrs[k] = serializeAttribute(k, *v);
    }
  }

  // effects
  nlohmann::json effects = nlohmann::json::array();
  for (const auto& eff : comp.data.effects) {
    effects.push_back(std::string(magic_enum::enum_name(eff)));
  }

  // storage
  nlohmann::json storage_json = nullptr;
  if (comp.storage) {
    nlohmann::json slots = nlohmann::json::array();
    int slots_used = 0;
    std::unordered_map<std::string, int> totals_by_name;
    for (const auto& slot : comp.storage->slots) {
      nlohmann::json slot_json = {{"id", slot.id}};
      if (slot.stack) {
        slots_used++;
        totals_by_name[slot.stack->item.name] += slot.stack->amount;
        slot_json["stack"] = {
          {"item", slot.stack->item.name},
          {"amount", slot.stack->amount},
          {"max", slot.stack->item.stack}
        };
      } else {
        slot_json["stack"] = nullptr;
      }
      slots.push_back(slot_json);
    }

    nlohmann::json top_items = nlohmann::json::array();
    if (!totals_by_name.empty()) {
      std::vector<std::pair<std::string, int>> totals;
      totals.reserve(totals_by_name.size());
      for (const auto& [name, amount] : totals_by_name) {
        totals.emplace_back(name, amount);
      }
      std::sort(totals.begin(), totals.end(),
                [](const auto& a, const auto& b) {
                  if (a.second != b.second) return a.second > b.second;
                  return a.first < b.first;
                });
      size_t n = std::min<size_t>(3, totals.size());
      for (size_t i = 0; i < n; i++) {
        top_items.push_back({{"name", totals[i].first}, {"amount", totals[i].second}});
      }
    }

    storage_json = {
      {"slots_count", comp.storage->slotsCount},
      {"slots_total", comp.storage->slotsCount},
      {"slots_used", slots_used},
      {"top_items", top_items},
      {"slots", slots}
    };
  }

  nlohmann::json json = {
    {"id", comp.data.id},
    {"name", comp.data.name},
    {"description", comp.data.description},
    {"icon", comp.data.icon},
    {"type", type_str},
    {"state", std::string(magic_enum::enum_name(comp.state))},
    {"size", std::string(magic_enum::enum_name(comp.size))},
    {"material", std::string(magic_enum::enum_name(comp.material))},
    {"error", comp.error},
    {"effects", effects},
    {"storage", storage_json},
    {"attributes", attrs},
    {"metadata", serializeMetadata(comp.data)}
  };

  if (!comp.require.empty()) {
    nlohmann::json req = nlohmann::json::array();
    for (const auto& r : comp.require) {
      req.push_back(r);
    }
    json["require"] = req;
  }

  return json;
}

nlohmann::json serializeConnection(entt::entity entity, const Connection& conn,
                                   const ItemsSystem* items) {
  (void)entity;
  nlohmann::json j = {
    {"id", conn.data.id},
    {"source", conn.source},
    {"target", conn.target},
    {"type", std::string(magic_enum::enum_name(conn.type))},
    {"medium", std::string(magic_enum::enum_name(conn.medium))}
  };
  if (items != nullptr && conn.type == ConnectionType::CONVEYOR) {
    j["transfer_progress"] = items->conveyorTransferProgress(conn.data.id);
  }
  return j;
}

// Legacy overload kept for backward compat — uses data.id for id
nlohmann::json serializeConnection(const Connection& conn) {
  return {
    {"id", conn.data.id},
    {"source", conn.source},
    {"target", conn.target},
    {"type", std::string(magic_enum::enum_name(conn.type))},
    {"medium", std::string(magic_enum::enum_name(conn.medium))}
  };
}

nlohmann::json serializeFrame(const Frame& frame) {
  nlohmann::json comps = nlohmann::json::array();
  for (const auto& c : frame.components) {
    if (c) {
      comps.push_back(serializeComponent(*c));
    }
  }

  // temperature from "temp" attribute final_value, or 0
  float temperature = 0.0f;
  auto it = frame.data.attributes.find("temp");
  if (it != frame.data.attributes.end() && it->second) {
    auto fv = it->second->GetFinalValue();
    if (std::holds_alternative<float>(fv)) {
      temperature = std::get<float>(fv);
    } else if (std::holds_alternative<int>(fv)) {
      temperature = static_cast<float>(std::get<int>(fv));
    }
  }

  return {
    {"id", frame.data.id},
    {"name", frame.data.name},
    {"size", std::string(magic_enum::enum_name(frame.size))},
    {"material", std::string(magic_enum::enum_name(frame.material))},
    {"temperature", temperature},
    {"components", comps},
    {"effects", nlohmann::json::array()},
    {"metadata", serializeMetadata(frame.data)}
  };
}

nlohmann::json serializeCanvasBadges(const Frame& frame, const PowerInfo* powerInfo) {
  nlohmann::json badges = nlohmann::json::object();

  // has_error: true if any component has a non-empty error string
  bool has_error = false;
  bool any_bad_state = false;
  for (const auto& c : frame.components) {
    if (!c) continue;
    if (!c->error.empty()) has_error = true;
    if (c->state == ComponentState::COMP_ERROR ||
        c->state == ComponentState::DESTROYED ||
        c->state == ComponentState::BROKEN) {
      any_bad_state = true;
    }
  }
  badges["has_error"] = has_error;

  // health: ok/warn/bad
  if (has_error || any_bad_state) {
    badges["health"] = "bad";
  } else {
    // check for blocked components as warning
    bool any_blocked = false;
    for (const auto& c : frame.components) {
      if (c && c->state == ComponentState::BLOCKED) any_blocked = true;
    }
    badges["health"] = any_blocked ? "warn" : "ok";
  }

  // temperature from "temp" attribute
  auto it = frame.data.attributes.find("temp");
  if (it != frame.data.attributes.end() && it->second) {
    auto fv = it->second->GetFinalValue();
    if (std::holds_alternative<float>(fv)) {
      badges["temperature"] = std::get<float>(fv);
    } else if (std::holds_alternative<int>(fv)) {
      badges["temperature"] = static_cast<float>(std::get<int>(fv));
    }
  }

  // power: determine from PowerInfo networks
  std::string power_status = "offline";
  if (powerInfo) {
    for (const auto& net : powerInfo->networks) {
      bool in_network = false;
      for (auto fid : net.frames) {
        if (fid == frame.data.id) { in_network = true; break; }
      }
      if (!in_network) continue;

      float net_prod = net.production - net.consumption;
      if (net_prod > 0.01f) {
        power_status = "surplus";
      } else if (net_prod < -0.01f) {
        power_status = "deficit";
      } else {
        power_status = "balanced";
      }
      break;
    }
  }
  badges["power"] = power_status;

  return badges;
}

nlohmann::json serializeFrame(entt::registry& registry, entt::entity entity, const Frame& frame) {
  auto j = serializeFrame(frame);
  j["entity_id"] = static_cast<int>(entity);

  // read wl::transform for position
  if (registry.all_of<wl::transform>(entity)) {
    auto& t = registry.get<wl::transform>(entity);
    j["position"] = {{"x", t.position.x}, {"y", t.position.y}};
  } else {
    j["position"] = {{"x", 0.0f}, {"y", 0.0f}};
  }

  // canvas badges — get power info if available
  const PowerInfo* powerInfo = nullptr;
  if (entt::locator<PowerInfo>::has_value()) {
    powerInfo = &entt::locator<PowerInfo>::value();
  }
  j["canvas_badges"] = serializeCanvasBadges(frame, powerInfo);

  return j;
}

nlohmann::json serializeFrameSummary(entt::entity entity, const Frame& frame) {
  return {
    {"entity_id", static_cast<int>(entity)},
    {"id", frame.data.id},
    {"name", frame.data.name},
    {"size", std::string(magic_enum::enum_name(frame.size))},
    {"component_count", static_cast<int>(frame.components.size())}
  };
}

nlohmann::json serializeEnvironment(const Environment& env) {
  return {
    {"radioactivity", env.radioactivity},
    {"temperature", env.temperature},
    {"air_flow", env.airFlow},
    {"sun", env.sun},
    {"minutes", env.minutes},
    {"is_day", env.isDay},
    {"days", env.days}
  };
}

} // namespace rpc
