#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>

#include <game/attributes.hpp>
// TODO:: add serialization for attributes

struct Metadata {
  int id = -1;
  std::string name;
  std::string description;
  std::map<std::string, std::shared_ptr<Attribute>> attributes;

  template <class T> T get(std::string name) {
    return std::get<T>(attributes[name]->GetFinalValue());
  }

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(name, description);
  };
  template <class Archive> void load(Archive &ar) { ar(name, description); };
};

struct Component {
  Metadata data;
  std::string state = "idle";

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(data, state); };
  template <class Archive> void load(Archive &ar) { ar(data, state); };
};

struct Frame {
  Metadata data;
  std::vector<std::shared_ptr<Component>> components;

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(data, components);
  };
  template <class Archive> void load(Archive &ar) { ar(data, components); };
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
