#pragma once
#include <SFML/Graphics.hpp>
#include <utils/entt.hpp>

namespace wl {

struct color {
  uint8_t r = 255, g = 255, b = 255, a = 255;
  operator sf::Color() const { return sf::Color(r, g, b, a); }
  static color from_sf(sf::Color c) { return {c.r, c.g, c.b, c.a}; }
};

struct visual_state {
  bool selected = false;
  bool hovered = false;
  bool ghost = false;
  bool hidden = false;
};

struct rect {
  float width = 0.0f;
  float height = 0.0f;
  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) { ar(width, height); };
};

struct position {
  float x = 0.0f;
  float y = 0.0f;
  friend class cereal::access;
  template <class Archive> void serialize(Archive &ar) { ar(x, y); };
};

enum class shape_type { rectangle, circle };

struct shape {
  shape_type type;
  wl::rect rect;
  wl::color fill_color;
  wl::color outline_color = {0, 0, 0, 255};
  float stroke = 1.0f;
};

struct transform {
  bool relative = false;
  wl::position position;
  float scale = 1.0f;
  float rotation = 0.0f;
  std::string layer = "";
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(position, scale, rotation, relative, layer);
  };
  template <class Archive> void load(Archive &ar) {
    ar(position, scale, rotation, relative, layer);
  };
  void field_save(FieldOutputArchive &ar) const {
    FIELD(ar, position);
    FIELD(ar, scale);
    FIELD(ar, rotation);
    FIELD(ar, relative);
    FIELD(ar, layer);
  }
  void field_load(FieldInputArchive &ar) {
    FIELD(ar, position);
    FIELD(ar, scale);
    FIELD(ar, rotation);
    FIELD(ar, relative);
    FIELD(ar, layer);
  }
};

struct text {
  std::string content = "";
  wl::color color;
  int size = 12;
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(content, size); };
  template <class Archive> void load(Archive &ar) { ar(content, size); };
  void field_save(FieldOutputArchive &ar) const {
    FIELD(ar, content);
    FIELD(ar, size);
  }
  void field_load(FieldInputArchive &ar) {
    FIELD(ar, content);
    FIELD(ar, size);
  }
};
struct sprite {
  std::string key = "";
  wl::color color;
  wl::rect rect;
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(key, rect); };
  template <class Archive> void load(Archive &ar) { ar(key, rect); };
  void field_save(FieldOutputArchive &ar) const {
    FIELD(ar, key);
    FIELD(ar, rect);
  }
  void field_load(FieldInputArchive &ar) {
    FIELD(ar, key);
    FIELD(ar, rect);
  }
};

struct line {
  wl::position position1;
  wl::position position2;
  std::string layer = "";
  float thickness = 1.0f;
  wl::color color;
};
}; // namespace wl
