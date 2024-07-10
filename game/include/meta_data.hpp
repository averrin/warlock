#include <utils/data/container.hpp>
#include <meta_data_store.hpp>

class MetaData: public Container<MetaDataStore> {
public:
  int8_t version = 1;
  int8_t type = 1;

  std::map<std::string, float> probability;
  std::map<std::string, std::map<std::string, float>> mapFeatures;
  std::map<int, std::map<std::string, float>> spawnTables;

  void combine(std::shared_ptr<MetaDataStore> store) override {
    for (auto [k, v] : store->probability) {
      probability[k] = v;
    }
    for (auto [k, v] : store->mapFeatures) {
      mapFeatures[k] = v;
    }
  }

  MetaData(): Container<MetaDataStore>(type, version) {}
};
