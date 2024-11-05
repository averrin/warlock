#pragma once
#include <SFML/Graphics.hpp>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
namespace fs = std::filesystem;

class AssetLoader {
public:
  AssetLoader(const std::string &assetsPath) { loadAssets(assetsPath); }

  const std::map<std::string, std::shared_ptr<sf::Texture>> &
  getTextures() const {
    return textures;
  }

private:
  std::map<std::string, std::shared_ptr<sf::Texture>> textures;

  void loadAssets(const std::string &assetsPath) {
    for (const auto &entry : std::filesystem::directory_iterator(assetsPath)) {
      if (entry.is_directory()) {
        loadAssets(entry.path().string());
      } else if (entry.is_regular_file()) {
        auto texture = std::make_shared<sf::Texture>();
        if (texture->loadFromFile(entry.path().string())) {
          textures[entry.path().filename().string()] = texture;
        }
      }
    }
    textures[""] = textures["cube.png"];
  }
};
