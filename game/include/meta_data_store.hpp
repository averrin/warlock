#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
namespace fs = std::filesystem;

#include <utils/data/store.hpp>

class MetaDataStore : public Store {
  friend class cereal::access;
  template <class Archive> void load(Archive &ar) {
    ar(cereal::base_class<Store>(this), probability, mapFeatures, spawnTables);
  };
  template <class Archive> void save(Archive &ar) const {
    ar(cereal::base_class<Store>(this), probability, mapFeatures, spawnTables);
  };

public:
  std::map<std::string, float> probability;
  std::map<std::string, std::map<std::string, float>> mapFeatures;
  std::map<int, std::map<std::string, float>> spawnTables;

  using Store::Store;
};
