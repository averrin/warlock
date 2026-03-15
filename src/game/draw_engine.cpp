#include <chrono>
#include <game/draw_engine.hpp>
#include <game/state.hpp>
#include <libcolor/libcolor.hpp>
#include <thread>
#include <utils/assets_loader.hpp>
#include <utils/entt_draw.hpp>
#include <utils/entt_lua.hpp>
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.

DrawEngine::DrawEngine(std::shared_ptr<Viewport> viewport)
    : viewport(viewport) {
  _cache = std::make_shared<sf::RenderTexture>();
}
void DrawEngine::resize(sf::Vector2u size) { layers->resize(size); }

void DrawEngine::init() {
  auto label = "Initializing DrawEngine";
  log.start(label);
  started = false;

  layers = std::make_shared<LayersManager>();
  auto &lua = entt::locator<sol::state>::value();

  auto zIndex = 0;
  for (auto l : lua["draw"]["layers"].get<std::vector<sol::table>>()) {
    auto n = l["name"].get<std::string>();
    layers->layers[n] = std::make_shared<Layer>();
    layers->layers[n]->zIndex = zIndex;
    auto e = l["enabled"].get_or(true);
    layers->layers[n]->enabled = e;
    if (e) {
      log.var("Layer", n);
    }

    zIndex++;
  }
  started = true;

  auto &emitter = entt::locator<event_emitter>::value();
  emitter.connect<ready_event>([&](const auto &e, const auto &em) {
    if (e.component == "viewport") {
      auto c = LibColor::Color::fromHexString(
          viewport->colors["PALETTE"]["BACKGROUND"]);
      bgColor = sf::Color(c.r, c.g, c.b, c.a);
      started = true;
    }
  });

  log.stop(label);
}

void DrawEngine::serve() {}
void DrawEngine::start() {
  auto &lua = entt::locator<sol::state>::value();
  for (auto &l : layers->layers) {
    log.var("Layer", l.first);
  }

  auto settings = lua["gui"];
  auto main_font = settings["font"].get<std::string>();
  if (!font.loadFromFile(main_font)) {
    log.error("No font");
  }

  auto &current_state = entt::locator<State>::value();
  transform_observer.connect(current_state.registry,
                             entt::collector.update<wl::transform>());
  text_observer.connect(current_state.registry,
                        entt::collector.update<wl::text>());
  line_observer.connect(current_state.registry,
                        entt::collector.update<wl::line>());
  sprite_observer.connect(current_state.registry,
                          entt::collector.update<wl::sprite>());
  state_observer.connect(current_state.registry,
                         entt::collector.update<wl::visual_state>());
}

void DrawEngine::draw() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    count++;
    _draw();
  }
}

void DrawEngine::drawTexts() {
  auto &current_state = entt::locator<State>::value();

  for (auto &f : current_state.registry.view<wl::text>()) {
    if (std::find(ignored.begin(), ignored.end(), (int)f) != ignored.end()) {
      continue;
    }
    auto text = current_state.registry.get<wl::text>(f);
    auto transform = current_state.registry.get<wl::transform>(f);

    if (transform.relative) {
      auto parent = current_state.registry.get<wl::transform>(
          current_state.registry.get<wl::relation>(f).parent);
      transform.position.x = transform.position.x + parent.position.x;
      transform.position.y = transform.position.y + parent.position.y;
    }

    auto t = std::make_shared<sf::Text>();
    t->setFont(font); // font is a sf::Font
    t->setPosition(transform.position.x, transform.position.y);
    t->setString(text.content);
    t->setCharacterSize(text.size);
    t->setFillColor(text.color);
    layers->layers["overlay"]->draw(t, (int)f);
  }
}

void DrawEngine::drawLines() {
  auto &current_state = entt::locator<State>::value();
  for (auto &c : current_state.registry.view<wl::line>()) {
    if (std::find(ignored.begin(), ignored.end(), (int)c) != ignored.end()) {
      continue;
    }
    auto l = current_state.registry.get<wl::line>(c);

    sf::Vector2f source(l.position1.x, l.position1.y);
    sf::Vector2f target(l.position2.x, l.position2.y);

    float length = std::hypot(target.x - source.x, target.y - source.y);
    float angle =
        std::atan2(target.y - source.y, target.x - source.x) * 180 / M_PI;

    auto line = std::make_shared<sf::RectangleShape>(
        sf::Vector2f(length, l.thickness)); // 2px thickness
    line->setPosition(source);
    line->setRotation(angle);
    line->setFillColor(l.color); // Set the color as needed

    layers->layers[l.layer]->draw(line, (int)c);
  }
}

void DrawEngine::drawSprites() {
  auto &current_state = entt::locator<State>::value();
  auto assetsLoader = entt::locator<AssetLoader>::value();
  for (auto &c : current_state.registry.view<wl::sprite>()) {
    if (std::find(ignored.begin(), ignored.end(), (int)c) != ignored.end()) {
      continue;
    }
    auto transform = current_state.registry.get<wl::transform>(c);
    auto s = current_state.registry.get<wl::sprite>(c);

    if (transform.relative) {
      auto parent = current_state.registry.get<wl::transform>(
          current_state.registry.get<wl::relation>(c).parent);
      transform.position.x = transform.position.x + parent.position.x;
      transform.position.y = transform.position.y + parent.position.y;
    }

    auto sprite = std::make_shared<sf::Sprite>();
    auto texture = assetsLoader.getTextures().at(s.key);
    sprite->setTexture(*texture);
    if (current_state.registry.all_of<wl::visual_state>(c)) {
      auto state = current_state.registry.get<wl::visual_state>(c);
      if (state.ghost) {
        sprite->setColor(sf::Color(255, 255, 255, 128));
      } else if (state.selected) {
        sprite->setColor(sf::Color::Red);
      } else if (state.hovered) {
        sprite->setColor(sf::Color::Green);
      }
    }
    sprite->setPosition(transform.position.x, transform.position.y);

    sf::Vector2u textureSize = texture->getSize();

    float scaleX = transform.scale * (s.rect.width / textureSize.x);
    float scaleY = transform.scale * (s.rect.height / textureSize.y);

    sprite->setScale(scaleX, scaleY);

    sprite->setRotation(transform.rotation);

    layers->layers[transform.layer]->draw(sprite, (int)c);
  }
}

void DrawEngine::drawHitboxes() {
  auto &current_state = entt::locator<State>::value();
  for (auto &c : current_state.registry.view<wl::rect>()) {
    if (std::find(ignored.begin(), ignored.end(), (int)c) != ignored.end()) {
      continue;
    }
    auto transform = current_state.registry.get<wl::transform>(c);
    auto r = current_state.registry.get<wl::rect>(c);

    if (transform.relative) {
      auto parent = current_state.registry.get<wl::transform>(
          current_state.registry.get<wl::relation>(c).parent);
      transform.position.x = transform.position.x + parent.position.x;
      transform.position.y = transform.position.y + parent.position.y;
    }

    auto rect =
        std::make_shared<sf::RectangleShape>(sf::Vector2f(r.width, r.height));
    rect->setPosition(sf::Vector2f(transform.position.x, transform.position.y));
    rect->setFillColor(sf::Color::Transparent);
    rect->setOutlineColor(sf::Color::Red);
    rect->setOutlineThickness(2);

    layers->layers["hitboxes"]->draw(rect, (int)c);
  }
}

void DrawEngine::_draw() {
  if (!started) {
    log.error("DrawEngine not started");
    return;
  }

  std::lock_guard<std::mutex> guard(renderMutex);

  auto &current_state = entt::locator<State>::value();

  // TODO: change full redraw to partials
  transform_observer.each([&](const auto entity) { fullRedraw = true; });
  text_observer.each([&](const auto entity) { fullRedraw = true; });
  line_observer.each([&](const auto entity) { fullRedraw = true; });
  sprite_observer.each([&](const auto entity) { fullRedraw = true; });
  state_observer.each([&](const auto entity) { fullRedraw = true; });
  if (!fullRedraw) {
    return;
  }
  fullRedraw = false;

  ignored.clear();

  for (auto &c : current_state.registry.view<wl::visual_state>()) {
    auto state = current_state.registry.get<wl::visual_state>(c);
    if (state.hidden) {
      ignored.push_back((int)c);
      if (current_state.registry.all_of<wl::relation>(c)) {
        auto rel = current_state.registry.get<wl::relation>(c);
        for (auto &child : rel.children) {
          ignored.push_back((int)child);
        }
      }
    }
  }
  for (auto i : ignored) {
    for (auto layer : layers->layers) {
      layer.second->remove(i);
    }
  }

  drawTexts();
  drawLines();
  drawSprites();
  drawHitboxes();

  auto mode = sf::VideoMode::getDesktopMode();
  _cache->create(mode.width, mode.height);
  _cache->clear(sf::Color::Transparent);
  _cache->draw(*layers);
  _cache->display();
  cache.setTexture(_cache->getTexture());
}
