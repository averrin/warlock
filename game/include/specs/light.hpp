#pragma once

#include <cereal/types/string.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>


enum class LightType { NONE, CLEAR, FIRE, MAGIC, ACID, FROST, INHERIT };

struct LightSpec {
  friend class cereal::access;
  template<class Archive>
  void serialize(Archive & ar) {
      ar( distance, type, stable );
  };
  float distance;
  LightType type;
  bool stable = false;
};

namespace Glow {
const LightSpec NONE = {0, LightType::NONE};
}

namespace hf {
struct glow {
  float distance;
  LightType type;
  bool stable = false; // depricated
  int bright = 80;
  int flick = 5;
  bool passive = false;
  int pulse = 0;

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(distance, type, bright, flick, passive, pulse);
  };
  template <class Archive> void load(Archive &ar) {
    ar(distance, type, bright, flick, passive, pulse);
  };
};
}
