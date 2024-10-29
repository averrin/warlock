#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <game/attributes.hpp>
#include <map>
#include <ranges>
#include <stdexcept>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>

#include <game/components/items.hpp>

struct Effect {};
enum class EffectID {
  OVERHEAT,
  FREEZE,
};
struct Metadata {
  int id = -1;
  std::string name = "";
  std::string description = "";
  std::map<std::string, std::shared_ptr<Attribute>> attributes = {};
  std::vector<EffectID> effects = {};

  static int newId() {
    int new_id = entt::monostate<"id"_hs>{};
    new_id++;
    entt::monostate<"id"_hs>{} = new_id;
    return new_id;
  }
  Metadata() {}
  Metadata(const Metadata &other) {
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

  void addEffect(EffectID id) {
    if (std::ranges::find(effects, id) != effects.end()) {
      return;
    }
    effects.push_back(id);
  }

  void removeEffect(EffectID id) {
    if (std::ranges::find(effects, id) == effects.end()) {
      return;
    }
    effects.erase(std::ranges::remove(effects, id).begin(), effects.end());
  }

  template <class T> void set(std::string name, T val) {
    attributes[name]->SetBaseValue(val);
  }

  template <class T> T get(std::string name) {
    return std::get<T>(attributes[name]->GetFinalValue());
  }

  bool has(std::string key) { return attributes.find(key) != attributes.end(); }

  template <class T> T get_or(std::string name, T default_value) {
    if (attributes.find(name) == attributes.end()) {
      return default_value;
    }
    return std::get<T>(attributes[name]->GetFinalValue());
  }

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(id, name, description, attributes);
  };
  template <class Archive> void load(Archive &ar) {
    ar(id, name, description, attributes);
  };
};

struct Environment {
  float radioactivity = 0.0f;
  float temperature = 0.0f;
  float airFlow = 0.0f;
  float sun = 0.0f;
  int minutes = 0;
  bool isDay = false;
  int days = 0;
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(temperature, minutes, radioactivity, airFlow, sun, days);
  };
  template <class Archive> void load(Archive &ar) {
    ar(temperature, minutes, radioactivity, airFlow, sun);
  };

  std::map<int, std::deque<float>> temperatures = {};
  std::map<std::string, std::deque<float>> named_history = {};
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
  BLOCKED,
  BROKEN,
};

enum class ComponentSize { S, M, L };

enum class FrameSize { S, M, L, G };

class ItemStorage {
public:
  int slotsCount = 0;
  std::vector<ItemSlot> slots;

  ItemStorage(int slotsCount) : slotsCount(slotsCount) { reset(); }

  void reset() {
    slots.clear();
    for (int i = 0; i < slotsCount; i++) {
      auto slot = ItemSlot();
      slot.id = Metadata::newId();
      slots.push_back(slot);
    }
  }

  bool add(ItemStack stack) {
    for (auto &slot : slots) {
      if (slot.stack == nullptr) {
        slot.stack = std::make_shared<ItemStack>(stack);
        return true;
      } else if (slot.stack->item.name == stack.item.name) {
        if (slot.stack->amount + stack.amount > slot.stack->item.stack) {
          return false;
        }
        slot.stack->amount += stack.amount;
        return true;
      }
    }
    return false;
  }

  bool remove(ItemStack stack) {
    for (auto &slot : slots) {
      if (slot.stack == nullptr) {
        continue;
      }
      if (slot.stack->item.name == stack.item.name) {
        slot.stack->amount -= stack.amount;
        if (slot.stack->amount <= 0) {
          slot.stack = nullptr;
        }
        return true;
      }
    }
    return false;
  }
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
  sol::table api = {};
  Frame *frame = nullptr;
  ComponentMaterial material = ComponentMaterial::STEEL;

  std::shared_ptr<ItemStorage> storage = nullptr;

  bool activate() {
    if (data.get_or<bool>("passive", false)) {
      return false;
    }

    if (state == ComponentState::DEACTIVATED ||
        state == ComponentState::ERROR) {
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
    if (state == ComponentState::ACTIVE ||
        state == ComponentState::ACTIVATING || state == ComponentState::ERROR) {
      state = ComponentState::DEACTIVATING;
      time_switch = data.get_or<int>("activation_time", 0);
      next_state = ComponentState::DEACTIVATED;
      return true;
    }
    return false;
  }

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(data, size, state, require, conflict, material);
  };
  template <class Archive> void load(Archive &ar) {
    ar(data, size, state, require, conflict, material);
  };

  Component() {}
  Component(const Component &other) {
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
  std::vector<std::shared_ptr<Component>> components;
  FrameSize size = FrameSize::M;
  ComponentMaterial material = ComponentMaterial::STEEL;

  std::map<FrameSize, std::map<ComponentSize, uint>> limits = {
      {FrameSize::S, {{ComponentSize::S, 8}, {ComponentSize::M, 1}}},
      {FrameSize::M, {{ComponentSize::S, 16}, {ComponentSize::M, 2}}},
      {FrameSize::L,
       {{ComponentSize::S, 32}, {ComponentSize::M, 4}, {ComponentSize::L, 1}}},
      {FrameSize::G,
       {{ComponentSize::S, 32}, {ComponentSize::M, 8}, {ComponentSize::L, 4}}}};

  Frame() {}

  Frame(const Frame &other) {
    data = other.data;
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
    ar(data, components, size, material);
  };
  template <class Archive> void load(Archive &ar) {
    ar(data, components, size, material);
  };
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

std::shared_ptr<Component>
create_component_from_lua(sol::state &lua, const std::string &lua_file_path);

struct exec_lua_function {
  std::shared_ptr<Component> component;
  std::string function_name;
};

struct add_component {
  Frame &frame;
  std::string component_name;
};
