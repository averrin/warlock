#include <fstream>
#include <game/components/frame.hpp>
#include <game/components/items.hpp>
#include <game/data_link.hpp>
#include <game/oracle.hpp>
#include <game/systems/power.hpp>
#include <game/nexus_api.hpp>
#include <game/state.hpp>
#include <game/systems/code_execution.hpp>
#include <sol/sol.hpp>
#include <sstream>

namespace {

NexusApi g_nexus_api;

sol::table make_nexus_component_api_table(sol::state &lua) {
  sol::state_view L(lua);
  sol::table t = L.create_table();
  t["showToast"] = [](std::string message, std::string type) {
    g_nexus_api.showToast(std::move(message), std::move(type));
  };
  t["setMapMarker"] = [](float x, float y, std::string label, std::string color) {
    g_nexus_api.setMapMarker(x, y, std::move(label), std::move(color));
  };
  t["clearMapMarkers"] = []() { g_nexus_api.clearMapMarkers(); };
  t["removeMapMarker"] = [](std::string label) {
    g_nexus_api.removeMapMarker(std::move(label));
  };
  t["getMapMarkers"] = [](sol::this_state s) {
    return g_nexus_api.getMapMarkers(s);
  };
  t["setGlobalIndicator"] = [](std::string key, std::string label,
                               std::string value, std::string color) {
    g_nexus_api.setGlobalIndicator(std::move(key), std::move(label),
                                   std::move(value), std::move(color));
  };
  t["getMouseX"] = []() { return g_nexus_api.getMouseX(); };
  t["getMouseY"] = []() { return g_nexus_api.getMouseY(); };
  return t;
}

DataPacket parse_data_packet_table(sol::table t, int default_source) {
  DataPacket p;
  p.source = t.get_or("source", default_source);
  p.destination = t.get_or("destination", -1);
  p.body = t.get_or("body", std::string{});
  sol::object ho = t["headers"];
  if (ho.valid() && ho.get_type() == sol::type::table) {
    sol::table headers = ho;
    for (const auto &pair : headers) {
      if (pair.first.get_type() != sol::type::string ||
          pair.second.get_type() != sol::type::string)
        continue;
      p.headers[pair.first.as<std::string>()] =
          pair.second.as<std::string>();
    }
  }
  return p;
}

} // namespace

void refresh_component_apis(CodeExecutionSystem& exec) {
  auto& st = entt::locator<State>::value();
  for (auto e : st.registry.view<Frame>()) {
    auto& frame = st.registry.get<Frame>(e);
    sol::state& L = exec.getState(frame.data.id);
    for (auto& c : frame.components) {
      if (!c) continue;
      const std::string type = c->data.get_or<std::string>("type", "");
      if (type == "Nexus") {
        c->api = make_nexus_component_api_table(L);
        continue;
      }
      auto it = exec.sources.find(c->data.name);
      if (it == exec.sources.end()) continue;
      try {
        sol::table spec = L.load(it->second).call();
        sol::object api = spec["api"];
        if (api.valid() && api.get_type() == sol::type::table) {
          c->api = api.as<sol::table>();
        }
      } catch (...) {
        // Missing or invalid component script — leave api as default
      }
    }
  }
}

// Sol2 bindings for the classes
void register_bindings(sol::state &lua) {
  // AttributeType enum
  lua.new_enum("AttributeType", "INT", AttributeType::INT, "FLOAT",
               AttributeType::FLOAT, "STRING", AttributeType::STRING, "BOOL",
               AttributeType::BOOL);

  // AttributeEasingType enum
  lua.new_enum("AttributeEasingType", "NONE", AttributeEasingType::NONE,
               "JITTER", AttributeEasingType::JITTER, "SAW",
               AttributeEasingType::SAW, "SIN", AttributeEasingType::SIN,
               "RANDOM_STEP", AttributeEasingType::RANDOM_STEP);

  lua.new_enum("ComponentState", "DEACTIVATED", ComponentState::DEACTIVATED,
               "ACTIVATING", ComponentState::ACTIVATING, "ACTIVE",
               ComponentState::ACTIVE, "DEACTIVATING",
               ComponentState::DEACTIVATING, "ERROR", ComponentState::COMP_ERROR,
               "DESTROYED", ComponentState::DESTROYED, "BLOCKED",
               ComponentState::BLOCKED, "BROKEN", ComponentState::BROKEN);

  lua.new_enum("FrameSize", "S", FrameSize::S, "M", FrameSize::M, "L",
               FrameSize::L, "G", FrameSize::G, "XS", FrameSize::XS);

  lua.new_enum("ComponentSize", "S", ComponentSize::S, "M", ComponentSize::M,
               "L", ComponentSize::L);
  lua.new_enum("ComponentMaterial", "ALUMINIUM", ComponentMaterial::ALUMINIUM,
               "COPPER", ComponentMaterial::COPPER, "STEEL",
               ComponentMaterial::STEEL, "TITANIUM",
               ComponentMaterial::TITANIUM, "PLASTIC",
               ComponentMaterial::PLASTIC, "GLASS", ComponentMaterial::GLASS);

  lua.new_enum("ConnectionType", "POWER", ConnectionType::POWER, "DATA",
               ConnectionType::DATA, "CONVEYOR", ConnectionType::CONVEYOR);

  lua.new_usertype<Environment>("Environment", "temperature",
                                &Environment::temperature, "minutes",
                                &Environment::minutes);

  // AttributeEasing struct
  lua.new_usertype<AttributeEasing>(
      "AttributeEasing", "easing_type", &AttributeEasing::easing_type,
      "easing_range", &AttributeEasing::easing_range, "easing_period",
      &AttributeEasing::easing_period);

  // Attribute class
  lua.new_usertype<Attribute>(
      "Attribute",
      sol::constructors<Attribute(const std::string &, const std::string &,
                                  AttributeType, AttributeValue,
                                  AttributeEasing)>(),
      "GetTitle", &Attribute::GetTitle, "GetDescription",
      &Attribute::GetDescription, "GetType", &Attribute::GetType,
      "SetBaseValue", &Attribute::SetBaseValue, "GetBaseValue",
      &Attribute::GetBaseValue, "GetFinalValue", &Attribute::GetFinalValue,
      "SetEasing", &Attribute::SetEasing);

  // Metadata struct
  lua.new_usertype<Metadata>(
      "Metadata", "id", &Metadata::id, "name", &Metadata::name, "description",
      &Metadata::description, "attributes", &Metadata::attributes);

  auto oracle = entt::locator<Oracle>::value();
  lua.new_usertype<Component>(
      "Component", "data", &Component::data, "api", &Component::api, "state",
      &Component::state, "size", &Component::size, "require",
      &Component::require, "conflict", &Component::conflict, "activate",
      &Component::activate, "deactivate", &Component::deactivate, "storage",
      &Component::storage, "sendRaw",
      [](std::shared_ptr<Component> self, const std::string &s) {
        data_link_send_raw(self, s);
      },
      "send",
      [](std::shared_ptr<Component> self, sol::table packet_tbl) {
        auto p = parse_data_packet_table(packet_tbl, self->data.id);
        data_link_send_packet(self, std::move(p));
      },
      "injectRaw",
      [](std::shared_ptr<Component> self, const std::string &s) {
        data_link_inject_raw(self, s);
      },
      "injectPacket",
      [](std::shared_ptr<Component> self, sol::table packet_tbl) {
        auto p = parse_data_packet_table(packet_tbl, self->data.id);
        data_link_inject_packet(self, std::move(p));
      },
      "readRaw",
      [](std::shared_ptr<Component> self) -> sol::optional<std::string> {
        if (self->data_raw_inbox.empty())
          return sol::nullopt;
        std::string s = std::move(self->data_raw_inbox.front());
        self->data_raw_inbox.pop_front();
        return s;
      },
      "read",
      [](std::shared_ptr<Component> self,
         sol::this_state st) -> sol::optional<sol::table> {
        if (self->data_packet_inbox.empty())
          return sol::nullopt;
        DataPacket p = std::move(self->data_packet_inbox.front());
        self->data_packet_inbox.pop_front();
        sol::state_view L(st);
        sol::table t = L.create_table();
        t["source"] = p.source;
        t["destination"] = p.destination;
        sol::table headers = L.create_table();
        for (const auto &kv : p.headers)
          headers[kv.first] = kv.second;
        t["headers"] = headers;
        t["body"] = p.body;
        return t;
      },
      "getCounterpart",
      [](const std::shared_ptr<Component> &self) { return self->counterpart_id; },
      "queueDepthRaw",
      [](const std::shared_ptr<Component> &self) {
        return static_cast<int>(self->data_raw_inbox.size());
      },
      "queueDepthPacket",
      [](const std::shared_ptr<Component> &self) {
        return static_cast<int>(self->data_packet_inbox.size());
      });
  lua.new_usertype<NetworkInfo>(
      "NetworkInfo", "production", &NetworkInfo::production, "consumption",
      &NetworkInfo::consumption, "accumulated", &NetworkInfo::accumulated,
      "accumulated_available", &NetworkInfo::accumulated_available,
      "battery_count", &NetworkInfo::battery_count);

  lua.new_usertype<Frame>(
      "Frame", "data", &Frame::data, "components", &Frame::components,
      "getComponentByType", &Frame::getComponentByType, "getPowerInfo",
      [](Frame &frame) -> NetworkInfo {
        auto &info = entt::locator<PowerInfo>::value();
        for (auto &net : info.networks) {
          if (std::ranges::find(net.frames, frame.data.id) !=
              net.frames.end()) {
            return net;
          }
        }
        return NetworkInfo{};
      },
      "getStorages",
      [](Frame &frame) -> std::vector<std::shared_ptr<ItemStorage>> {
        auto storages = std::vector<std::shared_ptr<ItemStorage>>{};
        for (auto c : frame.components) {
          if (c->data.get<std::string>("type") == "Storage") {
            storages.push_back(c->storage);
          }
        }
        return storages;
      },
      "hasComponentType", &Frame::hasComponentType, "getComponentsByType",
      &Frame::getComponentsByType, "getComponentByName",
      &Frame::getComponentByName);

  lua.new_usertype<ItemDefinition>(
      "ItemDefinition", "name", &ItemDefinition::name, "description",
      &ItemDefinition::description, "stack", &ItemDefinition::stack);

  lua.new_usertype<ItemStack>("ItemStack", "item", &ItemStack::item, "amount",
                              &ItemStack::amount);
  lua.new_usertype<ItemStorage>(
      "ItemStorage", "slots", &ItemStorage::slots, "slotsCount",
      &ItemStorage::slotsCount, "take", &ItemStorage::take, "getStackByItem",
      &ItemStorage::getStackByItem, "transferTo", &ItemStorage::transferTo,
      "transferFrom", &ItemStorage::transferFrom);

  lua.new_usertype<RecipeDefinition>(
      "RecipeDefinition", "name", &RecipeDefinition::name, "description",
      &RecipeDefinition::description, "outputs", &RecipeDefinition::outputs,
      "inputs", &RecipeDefinition::inputs, "timeCost",
      &RecipeDefinition::timeCost, "powerCost", &RecipeDefinition::powerCost);

  lua.new_usertype<Oracle>("Oracle", "getWiredFrames", &Oracle::getWiredFrames);
  lua.set("oracle", oracle);

  lua.new_usertype<NexusApi>(
      "NexusApi",
      "showToast", &NexusApi::showToast,
      "setMapMarker", &NexusApi::setMapMarker,
      "clearMapMarkers", &NexusApi::clearMapMarkers,
      "removeMapMarker", &NexusApi::removeMapMarker,
      "getMapMarkers", &NexusApi::getMapMarkers,
      "setGlobalIndicator", &NexusApi::setGlobalIndicator,
      "getMouseX", &NexusApi::getMouseX,
      "getMouseY", &NexusApi::getMouseY
  );

  lua.set("nexus", &g_nexus_api);
}

// Function to create a Component from a Lua file
std::shared_ptr<Component>
create_component_from_lua(sol::state &lua, const std::string &lua_source) {
  // fmt::print("Lua source: {}\n", lua_source);
  auto component = std::make_shared<Component>();

  sol::table spec = lua.load(lua_source).call();

  component->data.id = Metadata::newId();
  component->data.name = spec["name"].get_or<std::string>("");
  component->data.description = spec["description"].get_or<std::string>("");

  // Set attributes
  sol::table attributes = spec["attributes"];
  for (const auto &pair : attributes) {
    std::string key = pair.first.as<std::string>();
    sol::table attr_data = pair.second.as<sol::table>();

    std::string title = attr_data["title"].get_or<std::string>("");
    std::string description = attr_data["description"].get_or<std::string>("");
    AttributeType type = attr_data["type"].get_or(AttributeType::STRING);
    AttributeValue value;

    switch (type) {
    case AttributeType::INT:
      value = attr_data["value"].get<int>();
      break;
    case AttributeType::FLOAT:
      value = attr_data["value"].get<float>();
      break;
    case AttributeType::STRING:
      value = attr_data["value"].get<std::string>();
      break;
    case AttributeType::BOOL:
      value = attr_data["value"].get<bool>();
      break;
    }

    AttributeEasing easing;
    if (attr_data["easing"].valid()) {
      easing.easing_type =
          attr_data["easing"]["type"].get_or(AttributeEasingType::NONE);
      easing.easing_range = attr_data["easing"]["range"].get<float>();
      easing.easing_period = attr_data["easing"]["period"].get<float>();
    }

    auto attribute =
        std::make_shared<Attribute>(title, description, type, value, easing);
    component->data.attributes[key] = attribute;
  }

  component->data.icon = spec["icon"].get_or<std::string>("");

  Attribute temp("Temperature", "Total frame temperature in Celsius",
                 AttributeType::FLOAT, -1000.0f);
  component->data.attributes["temp"] = std::make_shared<Attribute>(temp);

  Attribute eff("Efficiency", "Efficiency", AttributeType::FLOAT, 1.0f);
  component->data.attributes["efficiency"] = std::make_shared<Attribute>(eff);

  // Set state
  component->state = spec["state"].get_or(ComponentState::DEACTIVATED);
  component->size = spec["size"].get_or(ComponentSize::S);
  component->material = spec["material"].get_or(ComponentMaterial::STEEL);

  // Optional: component requirements (list of other component names on the same frame)
  if (spec["require"].valid() && spec["require"].get_type() == sol::type::table) {
    sol::table req = spec["require"];
    for (const auto& pair : req) {
      if (pair.second.is<std::string>()) {
        component->require.push_back(pair.second.as<std::string>());
      }
    }
  }

  component->api = spec["api"];

  if (component->data.get_or<std::string>("type", "") == "Nexus") {
    component->api = make_nexus_component_api_table(lua);
  }

  return component;
}
