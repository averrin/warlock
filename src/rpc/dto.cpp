#include <rpc/dto.hpp>
#include <magic_enum.hpp>

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

  return {
    {"id", comp.data.id},
    {"name", comp.data.name},
    {"type", type_str},
    {"state", std::string(magic_enum::enum_name(comp.state))},
    {"size", std::string(magic_enum::enum_name(comp.size))},
    {"attributes", attrs}
  };
}

nlohmann::json serializeConnection(entt::entity entity, const Connection& conn) {
  return {
    {"id", conn.data.id},
    {"source", conn.source},
    {"target", conn.target},
    {"type", std::string(magic_enum::enum_name(conn.type))}
  };
}

// Legacy overload kept for backward compat — uses data.id for id
nlohmann::json serializeConnection(const Connection& conn) {
  return {
    {"id", conn.data.id},
    {"source", conn.source},
    {"target", conn.target},
    {"type", std::string(magic_enum::enum_name(conn.type))}
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
    {"effects", nlohmann::json::array()}
  };
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
