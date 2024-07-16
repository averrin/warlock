#pragma once
#include <memory>
#include <vector>

template <typename StoreType> class Container {
  LibLog::Logger log = LibLog::Logger(fmt::color::orange, "Cont");
  virtual LibLog::Logger &getLog() { return log; }
  virtual int8_t getVersion() { return version; }
  virtual int8_t getType() { return type; }

public:
  std::vector<std::shared_ptr<StoreType>> stores;

  std::shared_ptr<StoreType> create(std::string name, fs::path path) {
    auto store =
        std::make_shared<StoreType>(getType(), name, path, getVersion());
    return store;
  }

  void add(std::shared_ptr<StoreType> store) {
    combine(store);
    stores.push_back(store);
  }
  // void remove(StoreType* store) {
  //   auto it = std::find(stores.begin(), stores.end(), store);
  //   if (it != stores.end()) {
  //     stores.erase(it);
  //   }
  // }
  int size() { return stores.size(); }
  void clear() { stores.clear(); }

  virtual void combine(std::shared_ptr<StoreType> store) = 0;

  int8_t type = 0;
  int8_t version = 0;

  Container(int8_t t, int8_t v) : type(t), version(v) {}
};
