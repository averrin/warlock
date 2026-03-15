#pragma once
#include <entt/entt.hpp>
using namespace entt::literals;
#include <map>
#include <string>
#include <vector>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

/*
struct event_emitter : entt::emitter<event_emitter> {
  std::map<entt::id_type, std::vector<std::function<void(void *)>>> handlers;

  template <typename Type>
  void connect(std::function<void(Type &, const event_emitter &)> func) {
    handlers[entt::type_id<Type>().hash()].push_back([func = std::move(func),
                                                      this](void *value) {
      func(*static_cast<Type *>(value), static_cast<event_emitter &>(*this));
    });
    on<Type>([&](Type &event, const event_emitter &emitter) {
      for (auto &handler : handlers[entt::type_id<Type>().hash()]) {
        handler(&event);
      }
    });
  }
};
*/

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

namespace wl {
struct relation {
  std::vector<entt::entity> children;
  entt::entity parent = entt::null;
  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) {
    ar(children, parent);
  };
};
}; // namespace wl

// namespace wl
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
};
struct ineditor {
  std::string name = "";
  std::string icon = "";
  bool selected = false;

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(icon); };
  template <class Archive> void load(Archive &ar) { ar(icon); };
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
};

struct pickable {
  // ItemCategory category;
  // bool identified = false;
  // int count;
  // std::string unidName;

  // friend class cereal::access;
  // template <class Archive> void save(Archive &ar) const {
  //   ar(category, identified, count, unidName);
  // };
  // template <class Archive> void load(Archive &ar) {
  //   ar(category, identified, count, unidName);
  // };
};
struct wearable {
  // WearableType wearableType = WearableType::INVALID;
  // int durability = -1;

  // friend class cereal::access;
  // template <class Archive> void save(Archive &ar) const {
  //   ar(wearableType, durability);
  // };
  // template <class Archive> void load(Archive &ar) {
  //   ar(wearableType, durability);
  // };
};

struct cell {
  // std::shared_ptr<Cell> cell = nullptr;
};

struct room {
  // std::shared_ptr<Room> room;
};

struct children {
  std::vector<entt::entity> children;
};
struct size {
  int width = 0;
  int height = 0;
};
struct wall {};
struct tags {
  std::vector<std::string> tags;
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(tags); };
  template <class Archive> void load(Archive &ar) { ar(tags); };
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

struct creature {};
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
};
struct player {};
struct vision {
  float distance = 0;
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(distance); };
  template <class Archive> void load(Archive &ar) { ar(distance); };
};

// TODO
struct usable {};
struct consumable {};
struct destructable {};
}; // namespace hf
