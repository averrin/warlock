#include <app/controls.hpp>
#include <fmt/format.h>
#include <utils/entt_lua.hpp>

Controls::Controls() {}
Controls::~Controls() {}

void Controls::processEvent(sf::Event event) {

  auto &lua = entt::locator<sol::state>::value();
  auto controls = lua["controls"];

  winPrefix = controls["win_prefix"].get_or<std::string>(winPrefix);
  altPrefix = controls["alt_prefix"].get_or<std::string>(altPrefix);
  ctrlPrefix = controls["ctrl_prefix"].get_or<std::string>(ctrlPrefix);
  shiftPrefix = controls["shift_prefix"].get_or<std::string>(shiftPrefix);
  keyDelim = controls["key_delim"].get_or<std::string>(keyDelim);

  auto combo = getKeyName(event.key.code, event.key.system, event.key.alt,
                          event.key.control, event.key.shift);
  auto &emitter = entt::locator<event_emitter>::value();

  emitter.publish(key_event{
      combo, getKeyName(event.key.code, false, false, false, false),
      event.key.system, event.key.alt, event.key.control, event.key.shift});

  auto mapping = controls["mapping"].get<std::map<std::string, sol::function>>();
  if (mapping.find(combo) != mapping.end()) {
    auto func = mapping[combo];
    if (func.valid()) {
      func();
    }
  }
}

std::string Controls::getKeyName(sf::Keyboard::Key key, bool sys, bool alt,
                                 bool control, bool shift) {

  std::string name;
  std::string mods;
  if (sys) {
    mods = winPrefix + keyDelim;
  }
  if (alt) {
    mods = fmt::format("{}{}{}", mods, altPrefix, keyDelim);
  }
  if (control) {
    mods = fmt::format("{}{}{}", mods, ctrlPrefix, keyDelim);
  }
  if (shift) {
    mods = fmt::format("{}{}{}", mods, shiftPrefix, keyDelim);
  }
  switch (key) {
  default:
  case sf::Keyboard::Unknown:
    name = "Unknown";
    break;
  case sf::Keyboard::A:
    name = "A";
    break;
  case sf::Keyboard::B:
    name = "B";
    break;
  case sf::Keyboard::C:
    name = "C";
    break;
  case sf::Keyboard::D:
    name = "D";
    break;
  case sf::Keyboard::E:
    name = "E";
    break;
  case sf::Keyboard::F:
    name = "F";
    break;
  case sf::Keyboard::G:
    name = "G";
    break;
  case sf::Keyboard::H:
    name = "H";
    break;
  case sf::Keyboard::I:
    name = "I";
    break;
  case sf::Keyboard::J:
    name = "J";
    break;
  case sf::Keyboard::K:
    name = "K";
    break;
  case sf::Keyboard::L:
    name = "L";
    break;
  case sf::Keyboard::M:
    name = "M";
    break;
  case sf::Keyboard::N:
    name = "N";
    break;
  case sf::Keyboard::O:
    name = "O";
    break;
  case sf::Keyboard::P:
    name = "P";
    break;
  case sf::Keyboard::Q:
    name = "Q";
    break;
  case sf::Keyboard::R:
    name = "R";
    break;
  case sf::Keyboard::S:
    name = "S";
    break;
  case sf::Keyboard::T:
    name = "T";
    break;
  case sf::Keyboard::U:
    name = "U";
    break;
  case sf::Keyboard::V:
    name = "V";
    break;
  case sf::Keyboard::W:
    name = "W";
    break;
  case sf::Keyboard::X:
    name = "X";
    break;
  case sf::Keyboard::Y:
    name = "Y";
    break;
  case sf::Keyboard::Z:
    name = "Z";
    break;
  case sf::Keyboard::Num0:
    name = "Num0";
    break;
  case sf::Keyboard::Num1:
    name = "Num1";
    break;
  case sf::Keyboard::Num2:
    name = "Num2";
    break;
  case sf::Keyboard::Num3:
    name = "Num3";
    break;
  case sf::Keyboard::Num4:
    name = "Num4";
    break;
  case sf::Keyboard::Num5:
    name = "Num5";
    break;
  case sf::Keyboard::Num6:
    name = "Num6";
    break;
  case sf::Keyboard::Num7:
    name = "Num7";
    break;
  case sf::Keyboard::Num8:
    name = "Num8";
    break;
  case sf::Keyboard::Num9:
    name = "Num9";
    break;
  case sf::Keyboard::Escape:
    name = "Escape";
    break;
  case sf::Keyboard::LSystem:
    name = "LSystem";
    break;
  case sf::Keyboard::RSystem:
    name = "RSystem";
    break;
  case sf::Keyboard::Menu:
    name = "Menu";
    break;
  case sf::Keyboard::LBracket:
    name = "LBracket";
    break;
  case sf::Keyboard::RBracket:
    name = "RBracket";
    break;
  case sf::Keyboard::SemiColon:
    name = "SemiColon";
    break;
  case sf::Keyboard::Comma:
    name = "Comma";
    break;
  case sf::Keyboard::Period:
    name = "Period";
    break;
  case sf::Keyboard::Quote:
    name = "Quote";
    break;
  case sf::Keyboard::Slash:
    name = "Slash";
    break;
  case sf::Keyboard::BackSlash:
    name = "BackSlash";
    break;
  case sf::Keyboard::Tilde:
    name = "Tilde";
    break;
  case sf::Keyboard::Equal:
    name = "Equal";
    break;
  case sf::Keyboard::Dash:
    name = "Dash";
    break;
  case sf::Keyboard::Space:
    name = "Space";
    break;
  case sf::Keyboard::Return:
    name = "Return";
    break;
  case sf::Keyboard::BackSpace:
    name = "BackSpace";
    break;
  case sf::Keyboard::Tab:
    name = "Tab";
    break;
  case sf::Keyboard::PageUp:
    name = "PageUp";
    break;
  case sf::Keyboard::PageDown:
    name = "PageDown";
    break;
  case sf::Keyboard::End:
    name = "End";
    break;
  case sf::Keyboard::Home:
    name = "Home";
    break;
  case sf::Keyboard::Insert:
    name = "Insert";
    break;
  case sf::Keyboard::Delete:
    name = "Delete";
    break;
  case sf::Keyboard::Add:
    name = "Add";
    break;
  case sf::Keyboard::Subtract:
    name = "Subtract";
    break;
  case sf::Keyboard::Multiply:
    name = "Multiply";
    break;
  case sf::Keyboard::Divide:
    name = "Divide";
    break;
  case sf::Keyboard::Left:
    name = "Left";
    break;
  case sf::Keyboard::Right:
    name = "Right";
    break;
  case sf::Keyboard::Up:
    name = "Up";
    break;
  case sf::Keyboard::Down:
    name = "Down";
    break;
  case sf::Keyboard::Numpad0:
    name = "Numpad0";
    break;
  case sf::Keyboard::Numpad1:
    name = "Numpad1";
    break;
  case sf::Keyboard::Numpad2:
    name = "Numpad2";
    break;
  case sf::Keyboard::Numpad3:
    name = "Numpad3";
    break;
  case sf::Keyboard::Numpad4:
    name = "Numpad4";
    break;
  case sf::Keyboard::Numpad5:
    name = "Numpad5";
    break;
  case sf::Keyboard::Numpad6:
    name = "Numpad6";
    break;
  case sf::Keyboard::Numpad7:
    name = "Numpad7";
    break;
  case sf::Keyboard::Numpad8:
    name = "Numpad8";
    break;
  case sf::Keyboard::Numpad9:
    name = "Numpad9";
    break;
  case sf::Keyboard::F1:
    name = "F1";
    break;
  case sf::Keyboard::F2:
    name = "F2";
    break;
  case sf::Keyboard::F3:
    name = "F3";
    break;
  case sf::Keyboard::F4:
    name = "F4";
    break;
  case sf::Keyboard::F5:
    name = "F5";
    break;
  case sf::Keyboard::F6:
    name = "F6";
    break;
  case sf::Keyboard::F7:
    name = "F7";
    break;
  case sf::Keyboard::F8:
    name = "F8";
    break;
  case sf::Keyboard::F9:
    name = "F9";
    break;
  case sf::Keyboard::F10:
    name = "F10";
    break;
  case sf::Keyboard::F11:
    name = "F11";
    break;
  case sf::Keyboard::F12:
    name = "F12";
    break;
  case sf::Keyboard::F13:
    name = "F13";
    break;
  case sf::Keyboard::F14:
    name = "F14";
    break;
  case sf::Keyboard::F15:
    name = "F15";
    break;
  case sf::Keyboard::Pause:
    name = "Pause";
    break;
  }
  return fmt::format("{}{}", mods, name);
}
