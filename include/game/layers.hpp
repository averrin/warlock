#pragma once

#include <SFML/Graphics.hpp>
#include <map>
#include <memory>

class Layer : public sf::Drawable {
  bool damaged = true;
  bool needUpdate = true;

public:
  Layer();
  bool enabled = true;
  std::map<int, std::shared_ptr<sf::Drawable>> children;
  int zIndex = 0;
  void clear();
  void update();
  void remove(int);
  void draw(std::shared_ptr<sf::Drawable> child, int id);

  std::shared_ptr<sf::RenderTexture> cache;
  void resize(sf::Vector2u size) {
    cache->create(size.x, size.y, sf::ContextSettings(0, 0, 4));
  }
  void invalidate() { damaged = true; }

private:
  virtual void draw(sf::RenderTarget &target, sf::RenderStates states) const;
};

class LayersManager : public sf::Drawable {
private:
  virtual void draw(sf::RenderTarget &target, sf::RenderStates states) const;

public:
  LayersManager();
  std::map<std::string_view, std::shared_ptr<Layer>> layers;
  unsigned int size() {
    auto n = 0;
    for (auto [k, l] : layers) {
      n += l->children.size();
    }
    return n;
  }

  void setLayerEnabled(std::string name, bool enabled) {
    layers[name]->enabled = enabled;
    if (enabled) {
      layers[name]->invalidate();
    }
  }

  void invalidate(std::string_view name) { layers[name]->invalidate(); }
  void invalidate() {
    for (auto [k, l] : layers) {
      l->invalidate();
    }
  }

  void resize(sf::Vector2u size) {
    for (auto [k, l] : layers) {
      l->resize(size);
    }
  }
};
