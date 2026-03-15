#include <rpc/dto.hpp>
#include <magic_enum.hpp>

namespace rpc {

nlohmann::json serializeAttributeValue(const AttributeValue& val) {
  return std::visit([](auto&& v) -> nlohmann::json { return v; }, val);
}

nlohmann::json serializeAttribute(const std::string& key, const Attribute& attr) {
  return {
    {"key", key},
    {"title", attr.GetTitle()},
    {"description", attr.GetDescription()},
    {"type", magic_enum::enum_name(attr.GetType())},
    {"baseValue", serializeAttributeValue(attr.GetBaseValue())},
    {"finalValue", serializeAttributeValue(attr.GetFinalValue())}
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
  nlohmann::json j;
  j["data"] = serializeMetadata(comp.data);
  j["state"] = std::string(magic_enum::enum_name(comp.state));
  j["size"] = std::string(magic_enum::enum_name(comp.size));
  j["material"] = std::string(magic_enum::enum_name(comp.material));
  j["error"] = comp.error;
  j["frameId"] = comp.frame_id;

  nlohmann::json reqs = nlohmann::json::array();
  for (const auto& r : comp.require) reqs.push_back(r);
  j["require"] = reqs;

  nlohmann::json conflicts = nlohmann::json::array();
  for (const auto& c : comp.conflict) conflicts.push_back(c);
  j["conflict"] = conflicts;

  return j;
}

nlohmann::json serializeConnection(const Connection& conn) {
  return {
    {"data", serializeMetadata(conn.data)},
    {"source", conn.source},
    {"target", conn.target},
    {"type", std::string(magic_enum::enum_name(conn.type))}
  };
}

nlohmann::json serializeFrame(const Frame& frame) {
  nlohmann::json j;
  j["data"] = serializeMetadata(frame.data);
  j["size"] = std::string(magic_enum::enum_name(frame.size));
  j["material"] = std::string(magic_enum::enum_name(frame.material));

  nlohmann::json comps = nlohmann::json::array();
  for (const auto& c : frame.components) {
    if (c) {
      comps.push_back(serializeComponent(*c));
    }
  }
  j["components"] = comps;

  return j;
}

nlohmann::json serializeFrameSummary(entt::entity entity, const Frame& frame) {
  return {
    {"entityId", static_cast<int>(entity)},
    {"id", frame.data.id},
    {"name", frame.data.name},
    {"size", std::string(magic_enum::enum_name(frame.size))},
    {"componentCount", static_cast<int>(frame.components.size())}
  };
}

nlohmann::json serializeEnvironment(const Environment& env) {
  return {
    {"radioactivity", env.radioactivity},
    {"temperature", env.temperature},
    {"airFlow", env.airFlow},
    {"sun", env.sun},
    {"minutes", env.minutes},
    {"isDay", env.isDay},
    {"days", env.days}
  };
}

} // namespace rpc
