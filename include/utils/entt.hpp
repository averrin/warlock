#pragma once
#include <entt/entt.hpp>
using namespace entt::literals;
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <utils/data/field_archive.hpp>

struct event_emitter : entt::emitter<event_emitter> {
  std::map<entt::id_type,
           std::vector<std::pair<size_t, std::function<void(void *)>>>>
      handlers;
  size_t handler_id_counter = 0;

  template <typename Type>
  size_t connect(std::function<void(Type &, const event_emitter &)> func) {
    auto id = entt::type_id<Type>().hash();
    size_t handler_id = handler_id_counter++;
    handlers[id].emplace_back(handler_id, [func = std::move(func),
                                           this](void *value) {
      func(*static_cast<Type *>(value), static_cast<event_emitter &>(*this));
    });
    on<Type>([this, id](Type &event, const event_emitter &emitter) {
      for (auto &handler : handlers[id]) {
        handler.second(&event);
      }
    });
    return handler_id;
  }

  template <typename Type> void disconnect(size_t handler_id) {
    auto id = entt::type_id<Type>().hash();
    auto &handler_list = handlers[id];
    handler_list.erase(std::remove_if(handler_list.begin(), handler_list.end(),
                                      [handler_id](const auto &pair) {
                                        return pair.first == handler_id;
                                      }),
                       handler_list.end());
    if (handler_list.empty()) {
      this->template erase<Type>();
    }
  }
};

struct init_event {
  std::string component;
};

struct ready_event {
  std::string component;
};

struct close_event {
  std::string reason;
};

struct key_event {
  std::string combo;
  std::string key;
  bool sys;
  bool alt;
  bool control;
  bool shift;
};

struct component_state_changed {
  int frame_id;
  int component_id;
  std::string component_name;
  int prev_state;
  int new_state;
  std::string reason;   // e.g. "activation_timer", "deactivation_timer", "lua_start", "lua_stop"
};

struct spendable_pool_changed_event {
  std::map<std::string, int64_t> amounts;
};

namespace wl {
struct relation {
  std::vector<entt::entity> children;
  entt::entity parent = entt::null;
  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(children, parent);
  };
  void field_save(FieldOutputArchive &ar) const {
    FIELD(ar, children);
    FIELD(ar, parent);
  }
  void field_load(FieldInputArchive &ar) {
    FIELD(ar, children);
    FIELD(ar, parent);
  }
};
}; // namespace wl

namespace hf {
struct ingame {
  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) { ar(); };
};

struct meta {
  std::string name = "";
  std::string description = "";
  std::string id = "";

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(name, description, id);
  };
  template <class Archive> void load(Archive &ar) {
    ar(name, description, id);
  };
  void field_save(FieldOutputArchive &ar) const {
    FIELD(ar, name);
    FIELD(ar, description);
    FIELD(ar, id);
  }
  void field_load(FieldInputArchive &ar) {
    FIELD(ar, name);
    FIELD(ar, description);
    FIELD(ar, id);
  }
};
struct ineditor {
  std::string name = "";
  std::string icon = "";
  bool selected = false;

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(icon); };
  template <class Archive> void load(Archive &ar) { ar(icon); };
  void field_save(FieldOutputArchive &ar) const { FIELD(ar, icon); }
  void field_load(FieldInputArchive &ar) { FIELD(ar, icon); }
};

struct position {
  int x = 0;
  int y = 0;
  int z = 0;
  bool movable = true;

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(x, y, z, movable);
  };
  template <class Archive> void load(Archive &ar) { ar(x, y, z, movable); };
};

struct visible {
  std::string type = "";
  std::string sign = "";
  bool hidden = false;
  bool seeThrough = true;
  bool passThrough = true;

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(type, sign, hidden, seeThrough, passThrough);
  };
  template <class Archive> void load(Archive &ar) {
    ar(type, sign, hidden, seeThrough, passThrough);
  };
  void field_save(FieldOutputArchive &ar) const {
    FIELD(ar, type);
    FIELD(ar, sign);
    FIELD(ar, hidden);
    FIELD(ar, seeThrough);
    FIELD(ar, passThrough);
  }
  void field_load(FieldInputArchive &ar) {
    FIELD(ar, type);
    FIELD(ar, sign);
    FIELD(ar, hidden);
    FIELD(ar, seeThrough);
    FIELD(ar, passThrough);
  }
};

struct children {
  std::vector<entt::entity> children;
};
struct size {
  int width = 0;
  int height = 0;
};
struct wall {
  void field_save(FieldOutputArchive &) const {}
  void field_load(FieldInputArchive &) {}
};
struct tags {
  std::vector<std::string> tags;
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(tags); };
  template <class Archive> void load(Archive &ar) { ar(tags); };
  void field_save(FieldOutputArchive &ar) const { FIELD(ar, tags); }
  void field_load(FieldInputArchive &ar) { FIELD(ar, tags); }
};

struct renderable {
  std::string spriteKey = "UNKNOWN";
  std::string fgColor = "#fff";
  bool hasBg = false;
  std::string bgColor = "#ffffff00";
  bool hasBorder = false;
  std::string borderColor = "#ffffff00";
  bool hidden = false;
  int zIndex = 0;
  std::string fgLayer = "entities";
  std::string bgLayer = "entitiesBg";
  std::string brdLayer = "entitiesBrd";

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(spriteKey, fgColor, hasBg, bgColor, hasBorder, borderColor, hidden,
       zIndex, fgLayer, bgLayer, brdLayer);
  };
  template <class Archive> void load(Archive &ar) {
    ar(spriteKey, fgColor, hasBg, bgColor, hasBorder, borderColor, hidden,
       zIndex, fgLayer, bgLayer, brdLayer);
  };
  void field_save(FieldOutputArchive &ar) const {
    FIELD(ar, spriteKey);
    FIELD(ar, fgColor);
    FIELD(ar, hasBg);
    FIELD(ar, bgColor);
    FIELD(ar, hasBorder);
    FIELD(ar, borderColor);
    FIELD(ar, hidden);
    FIELD(ar, zIndex);
    FIELD(ar, fgLayer);
    FIELD(ar, bgLayer);
    FIELD(ar, brdLayer);
  }
  void field_load(FieldInputArchive &ar) {
    FIELD(ar, spriteKey);
    FIELD(ar, fgColor);
    FIELD(ar, hasBg);
    FIELD(ar, bgColor);
    FIELD(ar, hasBorder);
    FIELD(ar, borderColor);
    FIELD(ar, hidden);
    FIELD(ar, zIndex);
    FIELD(ar, fgLayer);
    FIELD(ar, bgLayer);
    FIELD(ar, brdLayer);
  }
};

struct overwrite {
  std::string spriteKey = "UNKNOWN";
  std::string fgColor = "#fff";
  bool hasBg = false;
  std::string bgColor = "#ffffff00";
  bool hasBorder = false;
  std::string borderColor = "#ffffff00";
  bool hidden = false;
  int zIndex = 0;
};

struct creature {
  void field_save(FieldOutputArchive &) const {}
  void field_load(FieldInputArchive &) {}
};
struct obstacle {
  bool passThrough = false;
  bool seeThrough = false;
  bool interactive = false;
  int passAddCost = 0;
  int interactionCost = 0;
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(passThrough, seeThrough, interactive, passAddCost, interactionCost);
  };
  template <class Archive> void load(Archive &ar) {
    ar(passThrough, seeThrough, interactive, passAddCost, interactionCost);
  };
  void field_save(FieldOutputArchive &ar) const {
    FIELD(ar, passThrough);
    FIELD(ar, seeThrough);
    FIELD(ar, interactive);
    FIELD(ar, passAddCost);
    FIELD(ar, interactionCost);
  }
  void field_load(FieldInputArchive &ar) {
    FIELD(ar, passThrough);
    FIELD(ar, seeThrough);
    FIELD(ar, interactive);
    FIELD(ar, passAddCost);
    FIELD(ar, interactionCost);
  }
};
struct player {
  void field_save(FieldOutputArchive &) const {}
  void field_load(FieldInputArchive &) {}
};
struct vision {
  float distance = 0;
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(distance); };
  template <class Archive> void load(Archive &ar) { ar(distance); };
  void field_save(FieldOutputArchive &ar) const { FIELD(ar, distance); }
  void field_load(FieldInputArchive &ar) { FIELD(ar, distance); }
};

}; // namespace hf
