#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <deque>

#include <game/attributes.hpp>
#include <map>
#include <ranges>
#include <stdexcept>
#include <utils/data/field_archive.hpp>
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
  std::string icon = "";

  static int newId() {
    int new_id = entt::monostate<"id"_hs>{};
    new_id++;
    entt::monostate<"id"_hs>{} = new_id;
    return new_id;
  }

  /// After loading state from disk, bump the global id counter so newId() never
  /// reuses ids already present in the registry.
  static void reconcileGlobalIdCounter(entt::registry &registry);
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
    ar(id, name, description, attributes, icon);
  };
  template <class Archive> void load(Archive &ar) {
    ar(id, name, description, attributes, icon);
  };
};

struct Environment {
  float radioactivity = 0.0f;
  float temperature = 0.0f;
  float airFlow = 0.0f;
  float sun = 0.0f;
  float mouseX = 0.0f;
  float mouseY = 0.0f;
  int minutes = 0;
  bool isDay = false;
  int days = 0;
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(temperature, minutes, radioactivity, airFlow, sun, days);
  };
  template <class Archive> void load(Archive &ar) {
    ar(temperature, minutes, radioactivity, airFlow, sun, days);
  };
  void field_save(FieldOutputArchive &ar) const {
    FIELD(ar, temperature);
    FIELD(ar, minutes);
    FIELD(ar, radioactivity);
    FIELD(ar, airFlow);
    FIELD(ar, sun);
    FIELD(ar, days);
  }
  void field_load(FieldInputArchive &ar) {
    FIELD(ar, temperature);
    FIELD(ar, minutes);
    FIELD(ar, radioactivity);
    FIELD(ar, airFlow);
    FIELD(ar, sun);
    FIELD(ar, days);
  }

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
  COMP_ERROR,
  DESTROYED,
  BLOCKED,
  BROKEN,
};

enum class ComponentSize { S, M, L };

enum class FrameSize { S, M, L, G, XS };

class ItemStorage : public std::enable_shared_from_this<ItemStorage> {
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

  bool canAdd(const ItemStack &stack) const {
    int remaining_amount = stack.amount;
    for (const auto &slot : slots) {
      if (slot.stack == nullptr) {
        return true; // Found an empty slot
      } else if (slot.stack->item.name == stack.item.name) {
        int available_space = slot.stack->item.stack - slot.stack->amount;
        if (available_space > 0) {
          remaining_amount -= available_space;
          if (remaining_amount <= 0) {
            return true; // Enough space found
          }
        }
      }
    }
    return false; // Not enough space
  }

  bool canRemove(const ItemStack &stack) const {
    int remaining_amount = stack.amount;
    for (const auto &slot : slots) {
      if (slot.stack != nullptr && slot.stack->item.name == stack.item.name) {
        remaining_amount -= slot.stack->amount;
        if (remaining_amount <= 0) {
          return true; // Enough items found
        }
      }
    }
    return false; // Not enough items
  }

  bool add(ItemStack &stack) {
    if (!canAdd(stack))
      return false;
    for (auto &slot : slots) {
      if (slot.stack == nullptr) {
        slot.stack = std::make_shared<ItemStack>(stack);
        return true;
      } else if (slot.stack->item.name == stack.item.name) {
        int available_space = slot.stack->item.stack - slot.stack->amount;
        if (available_space > 0) {
          int amount_to_add = std::min(available_space, stack.amount);
          slot.stack->amount += amount_to_add;
          stack.amount -= amount_to_add;
          if (stack.amount == 0) {
            return true;
          }
        }
      }
    }
    return false;
  }

  bool remove(ItemStack &stack) {
    if (!canRemove(stack))
      return false;
    for (auto &slot : slots) {
      if (slot.stack == nullptr) {
        continue;
      }
      if (slot.stack->item.name == stack.item.name) {
        int amount_to_remove = std::min(slot.stack->amount, stack.amount);
        slot.stack->amount -= amount_to_remove;
        stack.amount -= amount_to_remove;
        if (slot.stack->amount <= 0) {
          slot.stack = nullptr;
        }
        if (stack.amount == 0) {
          return true;
        }
      }
    }
    return false;
  }

  bool transferTo(std::shared_ptr<ItemStorage> storage,
                  std::shared_ptr<ItemStack> stack) {
    auto slot = std::find_if(slots.begin(), slots.end(),
                             [=](ItemSlot s) { return s.stack == stack; });
    if (slot == slots.end()) {
      return false;
    }

    if (storage->add(*stack)) {
      slot->stack = nullptr;
      return true;
    }
    return false;
  }

  bool transferFrom(std::shared_ptr<ItemStorage> storage,
                    std::shared_ptr<ItemStack> stack) {
    return storage->transferTo(shared_from_this(), stack);
  }

  std::shared_ptr<ItemStack> getStackByItem(std::string itemName) {
    for (auto &slot : slots) {
      if (slot.stack != nullptr && slot.stack->item.name == itemName) {
        return slot.stack;
      }
    }
    return nullptr;
  }

  std::shared_ptr<ItemStack> take(int stackId) {
    for (auto &slot : slots) {
      if (slot.stack != nullptr && slot.id == stackId) {
        auto takenStack = slot.stack;
        slot.stack = nullptr;
        return takenStack;
      }
    }
    return nullptr;
  }
};

struct Frame;

struct DataPacket {
  int source = -1;
  int destination = -1;
  std::map<std::string, std::string> headers;
  std::string body;

  template <class Archive> void serialize(Archive &ar) {
    ar(source, destination, headers, body);
  }
};

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
  int frame_id = -1;
  ComponentMaterial material = ComponentMaterial::STEEL;

  std::shared_ptr<ItemStorage> storage = nullptr;

  int counterpart_id = -1;
  int counterpart_id_alt = -1;
  std::deque<std::string> data_raw_inbox;
  std::deque<DataPacket> data_packet_inbox;

  bool activate() {
    if (data.get_or<bool>("passive", false)) {
      return false;
    }

    if (state == ComponentState::DEACTIVATED ||
        state == ComponentState::COMP_ERROR) {
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
        state == ComponentState::ACTIVATING || state == ComponentState::COMP_ERROR) {
      state = ComponentState::DEACTIVATING;
      time_switch = data.get_or<int>("activation_time", 0);
      next_state = ComponentState::DEACTIVATED;
      return true;
    }
    return false;
  }

  bool repair() {
    if (state == ComponentState::COMP_ERROR || state == ComponentState::BROKEN) {
      error = "";
      state = ComponentState::DEACTIVATED;
      time_switch = -1;
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
    counterpart_id = -1;
    counterpart_id_alt = -1;
    data_raw_inbox.clear();
    data_packet_inbox.clear();
  };

  Component() {}
  Component(const Component &other) {
    data = other.data;
    size = other.size;
    state = other.state;
    require = other.require;
    conflict = other.conflict;
    api = other.api;
    data_raw_inbox = other.data_raw_inbox;
    data_packet_inbox = other.data_packet_inbox;
    counterpart_id = other.counterpart_id;
    counterpart_id_alt = other.counterpart_id_alt;
  }
};

struct Frame {
  Metadata data;
  std::vector<std::shared_ptr<Component>> components;
  FrameSize size = FrameSize::M;
  ComponentMaterial material = ComponentMaterial::STEEL;

  std::map<FrameSize, std::map<ComponentSize, unsigned int>> limits = {
      {FrameSize::XS, {{ComponentSize::S, 1}}},
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
    size = other.size;
    material = other.material;
  }

  bool hasComponentType(std::string type, bool needActive = false) {
    for (auto c : components) {
      if (c->data.get<std::string>("type") == type &&
          (!needActive || c->state == ComponentState::ACTIVE)) {
        return true;
      }
    }
    return false;
  }

  void addComponent(std::shared_ptr<Component> c) {
    c->frame_id = data.id;
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

  std::shared_ptr<Component> getComponentByType(std::string t) {
    for (auto &c : components) {
      if (c->data.get<std::string>("type") == t) {
        return c;
      }
    }
    return nullptr;
  }

  std::vector<std::shared_ptr<Component>>
  getComponentsByType(std::string t) {
    std::vector<std::shared_ptr<Component>> out;
    for (auto &c : components) {
      if (c->data.get<std::string>("type") == t) {
        out.push_back(c);
      }
    }
    return out;
  }

  std::shared_ptr<Component> getComponentByName(std::string n) {
    for (auto &c : components) {
      if (c->data.name == n) {
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
    // frame_id is not serialized on Component; only addComponent() sets it. Restore after load.
    for (auto &c : components) {
      if (c)
        c->frame_id = data.id;
    }
  };
  void field_save(FieldOutputArchive &ar) const {
    FIELD(ar, data);
    FIELD(ar, components);
    FIELD(ar, size);
    FIELD(ar, material);
  }
  void field_load(FieldInputArchive &ar) {
    FIELD(ar, data);
    FIELD(ar, components);
    FIELD(ar, size);
    FIELD(ar, material);
    for (auto &c : components) {
      if (c)
        c->frame_id = data.id;
    }
  }
};

enum class ConnectionType {
  POWER,
  DATA,
  CONVEYOR,
};

enum class ConnectionMedium {
  WIRE,
  WIRELESS,
  BEAM,
};

struct Connection {
  Metadata data;
  int source = -1;
  int target = -1;
  ConnectionType type = ConnectionType::POWER;
  ConnectionMedium medium = ConnectionMedium::WIRE;

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(data, source, target, type, medium);
  };
  template <class Archive> void load(Archive &ar) {
    ar(data, source, target, type, medium);
  };
  void field_save(FieldOutputArchive &ar) const {
    FIELD(ar, data);
    FIELD(ar, source);
    FIELD(ar, target);
    FIELD(ar, type);
    FIELD(ar, medium);
  }
  void field_load(FieldInputArchive &ar) {
    FIELD(ar, data);
    FIELD(ar, source);
    FIELD(ar, target);
    FIELD(ar, type);
    FIELD(ar, medium);
  }
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
