#pragma once
#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <filesystem>
#include <fstream>
#include <ios>
#include <liblog/liblog.hpp>
#include <libprint/libprint.hpp>
#include <sstream>
#include <string>
#include <utils/entt.hpp>
#include <vector>
namespace fs = std::filesystem;

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <game/registry_container.hpp>

class Loader {
  LibLog::Logger log = LibLog::Logger(fmt::color::orange, "LOAD");

  std::string getCurrentDateTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::tm *timeInfo = std::localtime(&currentTime);
    std::ostringstream dateTimeStream;
    dateTimeStream << std::put_time(timeInfo, "%Y-%m-%d %H:%M:%S");
    return dateTimeStream.str();
  }

public:
  void init(LibLog::Logger parentLog);

  template <typename ContainerType>
  bool load(ContainerType &container, std::vector<std::string> files) {
    fs::path PATH = entt::monostate<"path"_hs>{};
    for (auto &_file : files) {
      // if(state_cache.find(_file) != state_cache.end()) {
      //   log.debug("State @ {} got from cache", _file);
      //   container.add(store);
      //   continue;
      // }
      auto path = (PATH / _file).string();
      auto file = fs::relative(path, PATH).string();
      auto store_name = file.substr(file.find_last_of("/") + 1);
      auto store = container.create(store_name, path);

      log.var("Path", file);
      if (fs::exists(path) == false) {
        log.error("File not found: {}", path);
        log.debug("But store is created anyway");
        store->initEmpty();
      } else {
        std::ifstream ifs(path, std::ios::in | std::ios::binary);
        cereal::BinaryInputArchive iarchive(ifs);
        // cereal::JSONInputArchive iarchive(ifs);
        try {
          iarchive(*store);
          log.info("{}: {} (type: {}, ver: {})",
                   LibPrint::utils::green("󰟉 Load"),
                   LibPrint::utils::italic(store->name), store->type,
                   store->version);
        } catch (cereal::Exception &e) {
          log.error("Error loading {}: {}", path, e.what());
          continue;
        }
      }
      container.add(store);
      // state_cache[_file] = store;
    }
    return container.stores.size() > 0;
  }

  void saveStateToFile(RegistryContainer &container,
                               std::string path) {
    std::shared_ptr<RegistryStore> store = container.create("state", path);
    store->initEmpty();
    RegistryContainer::copyRegistry(container.registry, store->registry);
      store->attributes["saved_at"] = getCurrentDateTime();
      auto file =
          fs::relative(store->path, entt::monostate<"path"_hs>{}).string();
      // log.debug("Try to save {}", path);
      std::ofstream ofs(path, std::ios::out | std::ios::binary);
      cereal::BinaryOutputArchive oarchive(ofs);
      // cereal::JSONOutputArchive oarchive(ofs);
      log.var("Path", file);
      log.info(
          "{}: {} (type: {}, ver: {})",
          LibPrint::utils::color(fmt::terminal_color::bright_red, " Save"),
          LibPrint::utils::italic(store->name), store->type, store->version);
      oarchive(*store);
  }

  template <typename ContainerType> void save(ContainerType &container) {
    for (auto store : container.stores) {
      auto path = store->path.string();
      store->attributes["saved_at"] = getCurrentDateTime();
      auto file =
          fs::relative(store->path, entt::monostate<"path"_hs>{}).string();
      // log.debug("Try to save {}", path);
      std::ofstream ofs(path, std::ios::out | std::ios::binary);
      cereal::BinaryOutputArchive oarchive(ofs);
      // cereal::JSONOutputArchive oarchive(ofs);
      log.var("Path", file);
      log.info(
          "{}: {} (type: {}, ver: {})",
          LibPrint::utils::color(fmt::terminal_color::bright_red, " Save"),
          LibPrint::utils::italic(store->name), store->type, store->version);
      oarchive(*store);
    }
  }
};
