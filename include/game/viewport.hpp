#pragma once
#include <array>
#include <exception>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace fs = std::filesystem;
#include <SFML/Graphics.hpp>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#include <fmt/format.h>
#include <liblog/liblog.hpp>
#include <utils/jobs.hpp>

typedef std::array<int, 3> TileSpec;
struct TileSet {
  std::vector<std::string> maps;
  std::pair<int, int> size;
  int gap;
  std::map<std::string, TileSpec> sprites;
  std::map<std::string, std::vector<std::string>> spriteVariations;
  std::map<std::string, std::vector<std::string>> colorVariations;
  std::map<std::string, std::map<std::string, std::vector<float>>>
      colorWandering;
};

class Viewport {
  LibLog::Logger log = LibLog::Logger(fmt::color::red, "VP");

public:
  fs::path tilesetPath;
  std::shared_ptr<Job> startJob;
  ~Viewport() { tilesTextures.clear(); }
  sf::IntRect getTileRect(int x, int y) {
    return sf::IntRect((tileSet.size.first + tileSet.gap) * x,
                       (tileSet.size.second + tileSet.gap) * y,
                       tileSet.size.first, tileSet.size.second);
  }
  void init(LibLog::Logger &parentLog);

  void loadTileset(fs::path path);
  void saveTileset(fs::path path);
  Viewport();
  void start();
  Viewport(const Viewport &other) {
    tileSet = other.tileSet;
    colors = other.colors;
    tilesTextures = other.tilesTextures;
    scale = other.scale;
    width = other.width;
    height = other.height;
    view_x = other.view_x;
    view_y = other.view_y;
    view_z = other.view_z;
  }
  TileSet tileSet;
  json colors;

public:
  float scale = 1.f;
  int width = 100;
  int height = 50;
  bool started = false;

  int view_x = 0;
  int view_y = 0;
  int view_z = 0;

  std::vector<std::shared_ptr<sf::Texture>> tilesTextures;

  void setSize(std::pair<int, int> size) {
    width = size.first;
    height = size.second;
  }

  sf::Color getColor(std::string color);
  sf::Color getColor(std::string cat, std::string key);
  std::string getColorString(std::string cat, std::string key);

  std::shared_ptr<sf::Sprite> makeSprite(std::string cat, std::string key);
  std::shared_ptr<sf::Sprite> makeSprite(TileSpec spec);
};
