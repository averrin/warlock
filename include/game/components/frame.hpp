#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <game/attributes.hpp>
#include <stdexcept>
#include <map>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>

struct Metadata {
  int id = -1;
  std::string name = "";
  std::string description = "";
  std::map<std::string, std::shared_ptr<Attribute>> attributes = {};
  
  static int newId() {
    int new_id = entt::monostate<"id"_hs>{};
    new_id++;
    entt::monostate<"id"_hs>{} = new_id;
    return new_id;
  }
  Metadata() {
  }
  Metadata(const Metadata& other) {
    id = other.id;
    name = other.name;
    description = other.description;
    for (auto [k, v] : other.attributes) {
      attributes[k] = std::make_shared<Attribute>(*v);
      if (attributes[k] == nullptr) {
        throw std::runtime_error("attribute is fucked");
      }
    }
  }

  template <class T> void set(std::string name, T val) {
    attributes[name]->SetBaseValue(val);
  }

  template <class T> T get(std::string name) {
    return std::get<T>(attributes[name]->GetFinalValue());
  }

  template <class T> T get_or(std::string name, T default_value) {
    if(attributes.find(name) == attributes.end()) {
      return default_value;
    }
    return std::get<T>(attributes[name]->GetFinalValue());
  }

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(id, name, description, attributes);
  };
  template <class Archive> void load(Archive &ar) { ar(id, name, description, attributes); };
};


struct Environment {
  float radioactivity = 0.0f;
  float temperature = 0.0f;
  float airFlow = 0.0f;
  int minutes = 0;
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(temperature, minutes, radioactivity, airFlow);
  };
  template <class Archive> void load(Archive &ar) {
    ar(temperature, minutes, radioactivity, airFlow);
  };
};

enum class ComponentMaterial {
  ALUMINIUM,
  COPPER,
  STEEL,
  TITANIUM,
  PLASTIC,
  GLASS,
};

enum class ComponentState {
  DEACTIVATED,
  ACTIVATING,
  ACTIVE,
  DEACTIVATING,
  ERROR,
  DESTROYED,
  BLOCKED
};

enum class ComponentSize {
  S, M, L
};

enum class FrameSize {
  S, M, L, G
};

struct Frame;
struct Component {
  Metadata data;
  ComponentState state = ComponentState::DEACTIVATED;
  ComponentSize size = ComponentSize::S;
  std::vector<std::string> require = {};
  std::vector<std::string> conflict = {};
  std::string error = "";
  int time_switch = -1;
  ComponentState next_state = ComponentState::DEACTIVATED;
  // std::map<std::string, sol::function> api;
  sol::table api = {};
  Frame *frame = nullptr;
  ComponentMaterial material = ComponentMaterial::STEEL;

  bool activate() {
    if (data.get_or<bool>("passive", false)) {
      return false;
    }

    if (state == ComponentState::DEACTIVATED || state == ComponentState::ERROR) {
      error = "";
      state = ComponentState::ACTIVATING;
      time_switch = data.get_or<int>("activation_time", 0);
      next_state = ComponentState::ACTIVE;
      return true;
    }
    return false;
  }

  bool deactivate() {
    if (data.get_or<bool>("passive", false)) {
      return false;
    }
    if (state == ComponentState::ACTIVE || state == ComponentState::ACTIVATING || state == ComponentState::ERROR) {
      state = ComponentState::DEACTIVATING;
      time_switch = data.get_or<int>("activation_time", 0);
      next_state = ComponentState::DEACTIVATED;
      return true;
    }
    return false;
  }

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(data, size, state, require, conflict, material); };
  template <class Archive> void load(Archive &ar) { ar(data,size, state, require, conflict, material); };

  Component() {
  }
  Component(const Component& other) {
    data = other.data;
    size = other.size;
    state = other.state;
    require = other.require;
    conflict = other.conflict;
    api = other.api;
  }
};

struct Frame {
  Metadata data;
  std::map<ComponentSize, uint> component_limits = {};
  std::vector<std::shared_ptr<Component>> components;
  FrameSize size = FrameSize::S;
  ComponentMaterial material = ComponentMaterial::STEEL;
  
  Frame() {
    component_limits[ComponentSize::S] = 0;
    component_limits[ComponentSize::M] = 0;
    component_limits[ComponentSize::L] = 0;
  }

  Frame(const Frame& other) {
    data = other.data;
    component_limits = other.component_limits;
    for (auto &c : other.components) {
      components.push_back(c);
    }
  }

  void addComponent(std::shared_ptr<Component> c) {
    c->frame = this;
    components.push_back(c);
  }

  bool activate() {
    for (auto &c : components) {
      c->activate();
    }
    return true;
  }

  bool deactivate() {
    for (auto &c : components) {
      c->deactivate();
    }
    return true;
  }

  std::shared_ptr<Component> getComponentByType(std::string type) {
    for (auto &c : components) {
      if (c->data.get<std::string>("type") == type) {
        return c;
      }
    }
    return nullptr;
  }

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(data, component_limits, components, size, material);
  };
  template <class Archive> void load(Archive &ar) { ar(data, component_limits, components, size, material); };
};

struct Connection {
  Metadata data;
  int source;
  int target;

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(data, source, target);
  };
  template <class Archive> void load(Archive &ar) { ar(data, source, target); };
};

std::shared_ptr<Component> create_component_from_lua(sol::state& lua, const std::string& lua_file_path);

struct exec_lua_function {
  std::shared_ptr<Component> component;
  std::string function_name;
};

struct add_component {
  Frame& frame;
  std::string component_name;
};
