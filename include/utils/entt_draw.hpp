#pragma once
#include <SFML/Graphics.hpp>
#include <utils/entt.hpp>

namespace wl {

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
  sf::Color fill_color = sf::Color::White;
  sf::Color outline_color = sf::Color::Black;
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
};

struct text {
  std::string text = "";
  sf::Color color = sf::Color::White;
  int size = 12;
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(text, size); };
  template <class Archive> void load(Archive &ar) { ar(text, size); };
};
struct sprite {
  std::string sprite = "";
  sf::Color color = sf::Color::White;
  wl::rect rect;
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(sprite, rect); };
  template <class Archive> void load(Archive &ar) { ar(sprite, rect); };
};

struct line {
  wl::position position1;
  wl::position position2;
  std::string layer = "";
  float thickness = 1.0f;
  sf::Color color = sf::Color::White;
};
}; // namespace wl
