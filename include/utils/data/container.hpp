#pragma once
#include <vector>
#include <memory>

template <typename StoreType>
class Container {
public:
  std::vector<std::shared_ptr<StoreType>> stores;

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
