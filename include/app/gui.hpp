#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <liblog/liblog.hpp>
#include <app/editor.hpp>

class Gui {
  LibLog::Logger log = LibLog::Logger(fmt::color::pink, "GUI");
  sf::Clock deltaClock;

public:
  Gui();
  ~Gui();

  void init(LibLog::Logger &parentLog);
  void serve();

  std::vector<std::shared_ptr<Editor>> editors = {};

  std::vector<std::function<void()>> renders = {};
  std::vector<std::function<void()>> statusRenders = {};

  void drawDocking(float padding);
  void drawStatusBar(float width, float height, float pos_x, float pos_y);
};
