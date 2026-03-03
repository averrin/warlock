#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <game/layers.hpp>
#include <game/viewport.hpp>
#include <liblog/liblog.hpp>
#include <map>
#include <mutex>
#include <string>
#include <utils/entt.hpp>

class DrawEngine {
  entt::observer transform_observer;
  entt::observer text_observer;
  entt::observer line_observer;
  entt::observer sprite_observer;
  entt::observer state_observer;

  std::vector<int> ignored = {};

public:
  LibLog::Logger log = LibLog::Logger(fmt::color::light_green, "DRAW");
  sf::Color bgColor;

  std::map<std::string, std::shared_ptr<entt::observer>> observers;

  DrawEngine(std::shared_ptr<Viewport> viewport);
  // Copy constructor
  DrawEngine(const DrawEngine &other);
  ~DrawEngine() = default;
  std::shared_ptr<Viewport> viewport;

  bool started = false;

  void init(/*LibLog::Logger parentLog*/);
  void serve();
  void start();
  void draw();
  void _draw();
  void resize(sf::Vector2u size);
  sf::Texture getTexture();
  int count = 0;

  std::shared_ptr<Job> drawJob;

  std::mutex renderMutex;

  bool fullRedraw = true;
};
