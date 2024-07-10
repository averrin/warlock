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
#include <sstream>
#include <utils/entt.hpp>
#include <vector>
namespace fs = std::filesystem;

class Loader {
  LibLog::Logger log = LibLog::Logger(fmt::color::orange, "Loader");

public:
  void init(LibLog::Logger parentLog);

  template <typename ContainerType, typename StoreType>
  void load(ContainerType &container, std::vector<std::string> files) {
    fs::path PATH = entt::monostate<"path"_hs>{};
    for (auto &file : files) {
      auto path = (PATH / file).string();
      // log.debug("Try to open {}", path);
      std::ifstream ifs(path, std::ios::in | std::ios::binary);
      cereal::BinaryInputArchive iarchive(ifs);
      // cereal::JSONInputArchive iarchive(ifs);
      auto store_name = file.substr(file.find_last_of("/") + 1);
      auto store =
          std::make_shared<StoreType>(container.type, store_name, path, container.version);
      iarchive(*store);
      log.info("Loaded {} (type: {}, ver: {})", path, store->type,
               store->version);
      container.add(store);
    }
  }

  template <typename ContainerType> void save(ContainerType &container) {
    for (auto store : container.stores) {
      auto path = store->path.string();
      // log.debug("Try to save {}", path);
      std::ofstream ofs(path, std::ios::out | std::ios::binary);
      cereal::BinaryOutputArchive oarchive(ofs);
      // cereal::JSONOutputArchive oarchive(ofs);
      log.info("Saving {} (type: {}, ver: {})", path, store->type,
               store->version);
      oarchive(*store);
    }
  }
};
