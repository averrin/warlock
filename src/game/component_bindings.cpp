#include <fstream>
#include <game/components/frame.hpp>
#include <game/components/items.hpp>
#include <game/data_link.hpp>
#include <game/frame_world.hpp>
#include <game/oracle.hpp>
#include <game/systems/power.hpp>
#include <game/nexus_api.hpp>
#include <game/state.hpp>
#include <game/systems/code_execution.hpp>
#include <game/well_known_entities.hpp>
#include <nlohmann/json.hpp>
#include <sol/sol.hpp>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {

nlohmann::json sol_object_to_json(sol::object o);

nlohmann::json sol_table_to_json(sol::table t) {
  std::vector<std::pair<sol::object, sol::object>> pairs;
  for (const auto &kv : t) {
    pairs.push_back({kv.first, kv.second});
  }
  if (pairs.empty())
    return nlohmann::json::object();

  bool dense_array = true;
  std::vector<bool> seen;
  int max_idx = 0;
  for (const auto &pr : pairs) {
    if (pr.first.get_type() != sol::type::number) {
      dense_array = false;
      break;
    }
    double d = pr.first.as<double>();
    if (d != std::floor(d) || d < 1.0) {
      dense_array = false;
      break;
    }
    int idx = static_cast<int>(d);
    max_idx = std::max(max_idx, idx);
  }
  if (dense_array) {
    if (max_idx != static_cast<int>(pairs.size()))
      dense_array = false;
    else {
      seen.assign(static_cast<size_t>(max_idx) + 1, false);
      for (const auto &pr : pairs) {
        int idx = pr.first.as<int>();
        if (idx < 1 || idx > max_idx) {
          dense_array = false;
          break;
        }
        seen[static_cast<size_t>(idx)] = true;
      }
      if (dense_array) {
        for (int i = 1; i <= max_idx; ++i) {
          if (!seen[static_cast<size_t>(i)]) {
            dense_array = false;
            break;
          }
        }
      }
    }
  }
  if (dense_array) {
    std::sort(pairs.begin(), pairs.end(), [](const auto &a, const auto &b) {
      return a.first.as<int>() < b.first.as<int>();
    });
    nlohmann::json arr = nlohmann::json::array();
    for (const auto &pr : pairs)
      arr.push_back(sol_object_to_json(pr.second));
    return arr;
  }

  nlohmann::json obj = nlohmann::json::object();
  for (const auto &pr : pairs) {
    std::string key;
    if (pr.first.get_type() == sol::type::string)
      key = pr.first.as<std::string>();
    else if (pr.first.get_type() == sol::type::number)
      key = std::to_string(pr.first.as<int>());
    else
      continue;
    obj[key] = sol_object_to_json(pr.second);
  }
  return obj;
}

nlohmann::json sol_object_to_json(sol::object o) {
  switch (o.get_type()) {
  case sol::type::nil:
    return nullptr;
  case sol::type::boolean:
    return o.as<bool>();
  case sol::type::number: {
    double d = o.as<double>();
    if (d == std::floor(d) &&
        d >= static_cast<double>(std::numeric_limits<int>::min()) &&
        d <= static_cast<double>(std::numeric_limits<int>::max()))
      return static_cast<int>(d);
    return d;
  }
  case sol::type::string:
    return o.as<std::string>();
  case sol::type::table:
    return sol_table_to_json(o.as<sol::table>());
  default:
    return nullptr;
  }
}

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

void merge_inspector_meta_from_spec(Component& c, sol::table spec) {
  sol::table attributes = spec["attributes"];
  if (!attributes.valid() || attributes.get_type() != sol::type::table)
    return;
  for (const auto& pair : attributes) {
    if (pair.first.get_type() != sol::type::string)
      continue;
    std::string key = pair.first.as<std::string>();
    auto ait = c.data.attributes.find(key);
    if (ait == c.data.attributes.end() || !ait->second)
      continue;
    if (!pair.second.is<sol::table>())
      continue;
    sol::table attr_data = pair.second.as<sol::table>();
    sol::object insp = attr_data["inspector"];
    if (insp.valid() && insp.get_type() == sol::type::table)
      ait->second->inspector_meta = sol_table_to_json(insp.as<sol::table>());
    else
      ait->second->inspector_meta = nlohmann::json{};
  }
}

void apply_injected_attribute_inspectors(Component& c) {
  if (auto it = c.data.attributes.find("temp");
      it != c.data.attributes.end() && it->second)
    it->second->inspector_meta = nlohmann::json{{"precision", 1}};
  if (auto it = c.data.attributes.find("efficiency");
      it != c.data.attributes.end() && it->second)
    it->second->inspector_meta = nlohmann::json{};
}

// Inject data-link methods into a component's api table.
void inject_data_link_api(sol::state_view L, sol::table api, std::shared_ptr<Component> c) {
  api["sendRaw"] = [c](const std::string &s) {
    data_link_send_raw(c, s);
  };
  api["send"] = [c, &L](sol::table packet_tbl) {
    auto p = parse_data_packet_table(packet_tbl, c->data.id);
    data_link_send_packet(c, std::move(p));
  };
  api["injectRaw"] = [c](const std::string &s) {
    data_link_inject_raw(c, s);
  };
  api["injectPacket"] = [c, &L](sol::table packet_tbl) {
    auto p = parse_data_packet_table(packet_tbl, c->data.id);
    data_link_inject_packet(c, std::move(p));
  };
  api["readRaw"] = [c]() -> sol::optional<std::string> {
    if (c->data_raw_inbox.empty())
      return sol::nullopt;
    std::string s = std::move(c->data_raw_inbox.front());
    c->data_raw_inbox.pop_front();
    return s;
  };
  api["read"] = [c](sol::this_state st) -> sol::optional<sol::table> {
    if (c->data_packet_inbox.empty())
      return sol::nullopt;
    DataPacket p = std::move(c->data_packet_inbox.front());
    c->data_packet_inbox.pop_front();
    sol::state_view Lv(st);
    sol::table t = Lv.create_table();
    t["source"] = p.source;
    t["destination"] = p.destination;
    sol::table headers = Lv.create_table();
    for (const auto &kv : p.headers)
      headers[kv.first] = kv.second;
    t["headers"] = headers;
    t["body"] = p.body;
    return t;
  };
  api["queueDepthRaw"] = [c]() {
    return static_cast<int>(c->data_raw_inbox.size());
  };
  api["queueDepthPacket"] = [c]() {
    return static_cast<int>(c->data_packet_inbox.size());
  };
}

// Inject storage methods into a component's api table.
void inject_storage_api(sol::state_view L, sol::table api, std::shared_ptr<Component> c) {
  if (!c->storage) return;
  auto s = c->storage;
  api["getStorage"] = [s]() { return s; };
  api["slots"] = [s]() -> decltype(auto) { return (s->slots); };
  api["slotsCount"] = [s]() { return s->slotsCount; };
  api["take"] = [s](int stackId) {
    return s->take(stackId);
  };
  api["getStackByItem"] = [s](const std::string &item) {
    return s->getStackByItem(item);
  };
  api["transferTo"] = [s](std::shared_ptr<ItemStorage> target,
                          std::shared_ptr<ItemStack> stack) {
    return s->transferTo(target, stack);
  };
  api["transferFrom"] = [s](std::shared_ptr<ItemStorage> source,
                            std::shared_ptr<ItemStack> stack) {
    return s->transferFrom(source, stack);
  };
}

} // namespace

void sync_counterpart_attribute_impl(Component& c) {
  auto it = c.data.attributes.find("counterpart");
  if (it != c.data.attributes.end() && it->second) {
    it->second->SetBaseValue(c.counterpart_id);
  } else {
    auto a = std::make_shared<Attribute>(
        "Counterpart", "Connected counterpart component ID",
        AttributeType::INT, c.counterpart_id, AttributeEasing(),
        nlohmann::json{{"readonly", true}});
    c.data.attributes["counterpart"] = a;
  }
}

namespace {

// Load a component spec with frameWorld and environment available as upvalues.
// The source is wrapped so that closures capture these as locals, keeping them
// out of the global Lua namespace (blueprint code cannot see them).
sol::table load_component_spec(sol::state& L, const std::string& source) {
  auto& wk = entt::locator<WellKnownEntities>::value();
  auto& st = entt::locator<State>::value();
  auto frameWorld = entt::locator<FrameWorld>::value();

  // Temporarily set globals so the upvalue-capture preamble can grab them.
  L.set("_fw", frameWorld);
  bool has_env = wk.environment != entt::null &&
                 st.registry.valid(wk.environment) &&
                 st.registry.all_of<Environment>(wk.environment);
  if (has_env)
    L.set("_env", st.registry.get<Environment>(wk.environment));
  else
    L.set("_env", sol::nil);

  // Preamble: capture into locals so the returned api closures keep references.
  std::string wrapped =
      "local frameWorld = _fw; local environment = _env; _fw = nil; _env = nil\n" +
      source;
  sol::table spec = L.load(wrapped).call();

  // Clean up temporaries (preamble already nils them, but be safe).
  L.set("_fw", sol::nil);
  L.set("_env", sol::nil);

  return spec;
}

void refresh_component_apis_impl(CodeExecutionSystem& exec) {
  auto& st = entt::locator<State>::value();
  for (auto e : st.registry.view<Frame>()) {
    auto& frame = st.registry.get<Frame>(e);
    sol::state& L = exec.getState(frame.data.id);
    sol::state_view Lv(L);
    for (auto& c : frame.components) {
      if (!c) continue;

      // Sync counterpart attribute.
      sync_counterpart_attribute_impl(*c);

      if (c->data.has("alive_relays")) {
        c->api = make_nexus_component_api_table(L);
        // Still inject data-link and storage into the nexus api table.
        inject_data_link_api(Lv, c->api, c);
        inject_storage_api(Lv, c->api, c);
        try {
          auto it = exec.sources.find(c->data.name);
          if (it != exec.sources.end()) {
            sol::table spec = load_component_spec(L, it->second);
            merge_inspector_meta_from_spec(*c, spec);
          }
        } catch (...) {
        }
        apply_injected_attribute_inspectors(*c);
        continue;
      }
      auto it = exec.sources.find(c->data.name);
      if (it == exec.sources.end()) {
        // No script source — create a bare api table with injected methods.
        if (!c->api.valid() || c->api.get_type() != sol::type::table) {
          c->api = Lv.create_table();
        }
        inject_data_link_api(Lv, c->api, c);
        inject_storage_api(Lv, c->api, c);
        apply_injected_attribute_inspectors(*c);
        continue;
      }
      try {
        sol::table spec = load_component_spec(L, it->second);
        sol::object api = spec["api"];
        if (api.valid() && api.get_type() == sol::type::table) {
          c->api = api.as<sol::table>();
        } else {
          c->api = Lv.create_table();
        }
        // Inject base methods (data-link, storage) — spec api methods take
        // precedence because they were set first; we only fill in keys that
        // the spec didn't define.
        sol::table api_tbl = c->api;
        inject_data_link_api(Lv, api_tbl, c);
        inject_storage_api(Lv, api_tbl, c);
        merge_inspector_meta_from_spec(*c, spec);
      } catch (...) {
      }
      apply_injected_attribute_inspectors(*c);
    }
    if (auto it = frame.data.attributes.find("temp");
        it != frame.data.attributes.end() && it->second)
      it->second->inspector_meta = nlohmann::json{{"precision", 1}};
  }
}

} // namespace

void sync_all_counterpart_attributes() {
  auto& st = entt::locator<State>::value();
  for (auto e : st.registry.view<Frame>()) {
    auto& frame = st.registry.get<Frame>(e);
    for (auto& c : frame.components) {
      if (c) sync_counterpart_attribute_impl(*c);
    }
  }
}

void refresh_component_apis(CodeExecutionSystem& exec) {
  refresh_component_apis_impl(exec);
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
                                &Environment::minutes, "days",
                                &Environment::days);

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
      &Component::require, "conflict", &Component::conflict,
      // activate/deactivate/repair — dot-notation closures (no self required).
      "activate", sol::readonly_property([](Component& self) {
        // Return a closure that captures a pointer to the component.
        // The Component lives inside a shared_ptr owned by the Frame, so the
        // pointer remains valid for the frame's lifetime.
        Component* p = &self;
        return sol::as_function([p]() { return p->activate(); });
      }),
      "deactivate", sol::readonly_property([](Component& self) {
        Component* p = &self;
        return sol::as_function([p]() { return p->deactivate(); });
      }),
      "repair", sol::readonly_property([](Component& self) {
        Component* p = &self;
        return sol::as_function([p]() { return p->repair(); });
      }));
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
          if (c->storage) {
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
      &ItemDefinition::description, "stack", &ItemDefinition::stack,
      "spendable", &ItemDefinition::spendable, "tier", &ItemDefinition::tier);

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

  lua.new_usertype<Oracle>("Oracle");
  lua.set("oracle", oracle);

  lua.new_usertype<FrameWorld>(
      "FrameWorld", "scanAdjacent", &FrameWorld::scanAdjacent, "moveFrame",
      sol::overload(
          [](FrameWorld& fw, int id, std::string d) { return fw.moveFrame(id, std::move(d)); },
          [](FrameWorld& fw, int id, std::string d, float step, float speed) {
            return fw.moveFrame(id, std::move(d), step, speed);
          },
          [](FrameWorld& fw, int id, std::string d, float step, float speed, float extra_power) {
            return fw.moveFrame(id, std::move(d), step, speed, extra_power);
          }),
      "subcellStep", []() { return FrameWorld::subcellStep(); },
      "cellStep", []() { return FrameWorld::cellStep(); }, "nfcFrames", &FrameWorld::nfcFrames);
  // frameWorld is NOT set as a global — it is scoped to component API
  // definitions via load_component_spec().

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
  // nexus is NOT set as a global — access it through the Nexus component's api.

  // =========================================================================
  // Convenience helpers — cleaner dot-notation API for component scripts
  // =========================================================================

  // locator(frame, selector) — find the first component matching the selector.
  //
  // Supported selector formats:
  //   "type:Propulsion"   — by the component's 'type' attribute
  //   "name:Motor"        — by component name
  //   ".Propulsion"       — CSS-style shorthand for type
  //   "#42"               — by decimal component id
  //   "#0x2A"             — by hexadecimal component id (0x prefix)
  //   "Motor"             — bare string defaults to name search
  //   { type = "..." }    — table selector by type
  //   { name = "..." }    — table selector by name
  lua.set_function("locator",
    [](Frame& frame, sol::object selector) -> std::shared_ptr<Component> {
      if (selector.is<std::string>()) {
        const auto& s = selector.as<std::string>();
        if (s.empty()) return nullptr;
        if (s[0] == '.') return frame.getComponentByType(s.substr(1));
        if (s[0] == '#') {
          try {
            // base 0 auto-detects decimal ("42") or hex with prefix ("0x2A")
            int id = std::stoi(s.substr(1), nullptr, 0);
            for (auto& c : frame.components)
              if (c->data.id == id) return c;
          } catch (...) {}
          return nullptr;
        }
        auto pos = s.find(':');
        if (pos != std::string::npos) {
          auto field = s.substr(0, pos), value = s.substr(pos + 1);
          if (field == "type") return frame.getComponentByType(value);
          if (field == "name") return frame.getComponentByName(value);
        }
        return frame.getComponentByName(s);  // bare string → search by name
      }
      if (selector.is<sol::table>()) {
        sol::table tbl = selector.as<sol::table>();
        sol::object type_val = tbl["type"];
        if (type_val.valid() && type_val.is<std::string>())
          return frame.getComponentByType(type_val.as<std::string>());
        sol::object name_val = tbl["name"];
        if (name_val.valid() && name_val.is<std::string>())
          return frame.getComponentByName(name_val.as<std::string>());
      }
      return nullptr;
    });

  // locatorAll(frame, selector) — find all components matching the selector.
  // Uses the same selector formats as locator().
  lua.set_function("locatorAll",
    [](Frame& frame, sol::object selector) -> std::vector<std::shared_ptr<Component>> {
      if (selector.is<std::string>()) {
        const auto& s = selector.as<std::string>();
        if (s.empty()) return {};
        if (s[0] == '.') return frame.getComponentsByType(s.substr(1));
        auto pos = s.find(':');
        if (pos != std::string::npos) {
          auto field = s.substr(0, pos), value = s.substr(pos + 1);
          if (field == "type") return frame.getComponentsByType(value);
          if (field == "name") {
            auto c = frame.getComponentByName(value);
            return c ? std::vector<std::shared_ptr<Component>>{c}
                     : std::vector<std::shared_ptr<Component>>{};
          }
        }
        auto c = frame.getComponentByName(s);
        return c ? std::vector<std::shared_ptr<Component>>{c}
                 : std::vector<std::shared_ptr<Component>>{};
      }
      if (selector.is<sol::table>()) {
        sol::table tbl = selector.as<sol::table>();
        sol::object type_val = tbl["type"];
        if (type_val.valid() && type_val.is<std::string>())
          return frame.getComponentsByType(type_val.as<std::string>());
        sol::object name_val = tbl["name"];
        if (name_val.valid() && name_val.is<std::string>()) {
          auto c = frame.getComponentByName(name_val.as<std::string>());
          return c ? std::vector<std::shared_ptr<Component>>{c}
                   : std::vector<std::shared_ptr<Component>>{};
        }
      }
      return {};
    });

  // attr(obj, key)          — get an attribute's final (eased) value; nil if missing.
  // setAttr(obj, key, value) — set an attribute's base value.
  // Both work on any object that has .data.attributes (Component or Frame).
  lua.script(R"(
function attr(obj, key)
  local a = obj.data.attributes[key]
  if a then return a:GetFinalValue() end
  return nil
end

function setAttr(obj, key, value)
  local a = obj.data.attributes[key]
  if a then a:SetBaseValue(value) end
end
)");

  // Grid step constants — used internally by Propulsion; exposed globally so
  // blueprint scripts can reason about distances without hardcoding pixel values.
  lua.set_function("subcellStep", []() { return FrameWorld::subcellStep(); });
  lua.set_function("cellStep", []() { return FrameWorld::cellStep(); });
}

// Function to create a Component from a Lua file
std::shared_ptr<Component>
create_component_from_lua(sol::state &lua, const std::string &lua_source) {
  auto component = std::make_shared<Component>();

  sol::table spec = load_component_spec(lua, lua_source);

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

    nlohmann::json inspector_meta;
    sol::object insp = attr_data["inspector"];
    if (insp.valid() && insp.get_type() == sol::type::table)
      inspector_meta = sol_table_to_json(insp.as<sol::table>());
    auto attribute = std::make_shared<Attribute>(title, description, type, value, easing,
                                                 std::move(inspector_meta));
    component->data.attributes[key] = attribute;
  }

  component->data.icon = spec["icon"].get_or<std::string>("");

  Attribute temp("Temperature", "Total frame temperature in Celsius",
                 AttributeType::FLOAT, -1000.0f, AttributeEasing(),
                 nlohmann::json{{"precision", 1}});
  component->data.attributes["temp"] = std::make_shared<Attribute>(temp);

  Attribute eff("Efficiency", "Efficiency", AttributeType::FLOAT, 1.0f);
  component->data.attributes["efficiency"] = std::make_shared<Attribute>(eff);

  // counterpart attribute (populated later by data-link system)
  auto cp_attr = std::make_shared<Attribute>(
      "Counterpart", "Connected counterpart component ID",
      AttributeType::INT, -1, AttributeEasing(),
      nlohmann::json{{"readonly", true}});
  component->data.attributes["counterpart"] = cp_attr;

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

  if (component->data.has("alive_relays")) {
    component->api = make_nexus_component_api_table(lua);
  }

  return component;
}
