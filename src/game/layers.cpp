#include <SFML/Graphics.hpp>
#include <game/layers.hpp>

Layer::Layer() {
  cache = std::make_shared<sf::RenderTexture>();
  cache->setSmooth(true);
};
void Layer::draw(sf::RenderTarget &target, sf::RenderStates states) const {
  if (!enabled) {
    return;
  }
  sf::Sprite sprite;
  sprite.setTexture(cache->getTexture());
  target.draw(sprite, states);
};

void Layer::update() {
  if (damaged) {
    cache->clear(sf::Color::Transparent);
    for (auto [id, child] : children) {
      cache->draw(*child);
    }
    damaged = false;
    cache->display();
  }
}

void Layer::clear() {
  children.clear();
  invalidate();
}
void Layer::remove(int id) {
  children.erase(id);
  invalidate();
}
void Layer::draw(std::shared_ptr<sf::Drawable> child, int id) {
  children[id] = child;
  invalidate();
}

LayersManager::LayersManager(){};

void LayersManager::draw(sf::RenderTarget &target,
                         sf::RenderStates states) const {
  std::vector<std::shared_ptr<Layer>> _layers;
  for (auto [k, l] : layers) {
    _layers.push_back(l);
  };
  std::sort(_layers.begin(), _layers.end(),
            [](auto ll, auto lr) { return ll->zIndex < lr->zIndex; });
  for (auto l : _layers) {
    l->update();
    target.draw(*l, states);
  }
}
