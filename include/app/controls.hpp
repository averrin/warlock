#pragma once
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <string>

class Controls {
public:
  Controls();
  ~Controls();

  void processEvent(sf::Event event);
  std::string getKeyName(sf::Keyboard::Key key, bool sys, bool alt,
                         bool control, bool shift);

  std::string winPrefix = "M";
  std::string altPrefix = "A";
  std::string ctrlPrefix = "C";
  std::string shiftPrefix = "S";
  std::string keyDelim = "-";
};
