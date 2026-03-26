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
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <type_traits>

#include <game/registry_container.hpp>
#include <game/registry_store.hpp>

namespace loader_detail {
template <typename S>
void clearIfRegistryStore(const std::shared_ptr<S> &store) {
  if constexpr (std::is_same_v<S, RegistryStore>) {
    store->registry.clear();
  }
}
} // namespace loader_detail

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
  void init(LibLog::Logger &parentLog);

  template <typename ContainerType>
  bool load(ContainerType &container, std::vector<std::string> files) {
    fs::path PATH = entt::monostate<"path"_hs>{};
    for (auto &_file : files) {
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
        try {
          iarchive(*store);
          log.info("{}: {} (type: {}, ver: {})",
                   LibPrint::utils::green("󰟉 Load"),
                   LibPrint::utils::italic(store->name), store->type,
                   store->version);
        } catch (const std::exception &e) {
          log.error("Error loading {}: {} — using empty store", path, e.what());
          loader_detail::clearIfRegistryStore(store);
          store->initEmpty();
        }
      }
      container.add(store);
    }
    return container.stores.size() > 0;
  }

  void saveStateToFile(RegistryContainer &container, std::string path) {
    std::shared_ptr<RegistryStore> store = container.create("state", path);
    store->initEmpty();
    RegistryContainer::copyRegistry(container.registry, store->registry);
    store->attributes["saved_at"] = getCurrentDateTime();
    auto file =
        fs::relative(store->path, entt::monostate<"path"_hs>{}).string();
    log.var("Path", file);
    log.info(
        "{}: {} (type: {}, ver: {})",
        LibPrint::utils::color(fmt::terminal_color::bright_red, " Save"),
        LibPrint::utils::italic(store->name), store->type, store->version);
    try {
      fs::create_directories(fs::path(path).parent_path());
      std::ofstream ofs(path, std::ios::out | std::ios::binary);
      if (!ofs.is_open()) {
        log.error("Failed to open file for writing: {}", path);
        return;
      }
      cereal::BinaryOutputArchive oarchive(ofs);
      oarchive(*store);
    } catch (cereal::Exception &e) {
      log.error("Serialization error saving {}: {}", path, e.what());
    } catch (std::exception &e) {
      log.error("Error saving {}: {}", path, e.what());
    }
  }

  template <typename ContainerType> void save(ContainerType &container) {
    for (auto store : container.stores) {
      auto path = store->path.string();
      store->attributes["saved_at"] = getCurrentDateTime();
      auto file =
          fs::relative(store->path, entt::monostate<"path"_hs>{}).string();
      log.var("Path", file);
      log.info(
          "{}: {} (type: {}, ver: {})",
          LibPrint::utils::color(fmt::terminal_color::bright_red, " Save"),
          LibPrint::utils::italic(store->name), store->type, store->version);
      try {
        fs::create_directories(fs::path(path).parent_path());
        std::ofstream ofs(path, std::ios::out | std::ios::binary);
        if (!ofs.is_open()) {
          log.error("Failed to open file for writing: {}", path);
          continue;
        }
        cereal::BinaryOutputArchive oarchive(ofs);
        oarchive(*store);
      } catch (cereal::Exception &e) {
        log.error("Serialization error saving {}: {}", path, e.what());
      } catch (std::exception &e) {
        log.error("Error saving {}: {}", path, e.what());
      }
    }
  }
};
