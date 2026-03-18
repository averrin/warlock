#include <fstream>
#include <game/components/frame.hpp>
#include <game/components/items.hpp>
#include <game/oracle.hpp>
#include <game/systems/power.hpp>
#include <sol/sol.hpp>
#include <sstream>

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
               FrameSize::L, "G", FrameSize::G);

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
      &Component::storage);
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
      "hasComponentType", &Frame::hasComponentType, "getComponentByName",
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

  lua.new_usertype<Oracle>("Oracle", "getNFCFrames", &Oracle::getNFCFrames,
                           "getWiredFrames", &Oracle::getWiredFrames);
  lua.set("oracle", oracle);
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

  return component;
}
