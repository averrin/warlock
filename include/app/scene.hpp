#pragma once
#include <liblog/liblog.hpp>

#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include <app/controls.hpp>

class Scene {
  LibLog::Logger log = LibLog::Logger(fmt::color::light_green, "SCENE");
  std::shared_ptr<Controls> controls;

public:
  Scene();
  ~Scene();

  sf::RenderWindow *window;

  void init(LibLog::Logger parentLog);
  void processEvent(sf::Event event);
  void serve();
  void draw();
  bool acceptInput = true;
};

struct sf_event_event {
  sf::Event event;
};
