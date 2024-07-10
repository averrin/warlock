#include <components.hpp>
#include <iostream>
#include <registry_store.hpp>
#include <utils/data/container.hpp>
#include <utils/entt.hpp>

// Primary template for TypeVisitor
template <typename ComponentList, template <typename> class Visitor,
          std::size_t Index = 0>
struct TypeVisitor;

// Specialization for non-empty type list
template <template <typename> class Visitor, typename T, typename... Rest,
          std::size_t Index>
struct TypeVisitor<ComponentList<T, Rest...>, Visitor, Index> {
  static void visit(entt::entity entity, std::shared_ptr<RegistryStore> store,
                    entt::entity copy, entt::registry &dest) {
    Visitor<T>::visit(entity, store, copy, dest, Index);
    TypeVisitor<ComponentList<Rest...>, Visitor, Index + 1>::visit(
        entity, store, copy, dest);
  }
};

// Specialization for empty type list (end of recursion)
template <template <typename> class Visitor, std::size_t Index>
struct TypeVisitor<ComponentList<>, Visitor, Index> {
  static void visit(entt::entity entity, std::shared_ptr<RegistryStore> store,
                    entt::entity copy, entt::registry &dest) {}
};

template <typename T> struct EmplaceVisitor {
  static void visit(entt::entity entity, std::shared_ptr<RegistryStore> store,
                    entt::entity copy, entt::registry &dest,
                    std::size_t index) {
    // std::cout << "Visiting type at index " << index << ": " <<
    // typeid(T).name() << std::endl;

    if (store->registry.all_of<T>(entity)) {
      dest.storage<T>().push(copy, store->registry.storage<T>().value(entity));
    }
  }
};

class Prototypes : public Container<RegistryStore> {
public:
  int8_t version = 1;
  int8_t type = 2;

  entt::registry registry;

  void combine(std::shared_ptr<RegistryStore> store) override {
    for (auto entity : store->registry.view<hf::meta>()) {
      auto copy = registry.create();
      TypeVisitor<all_components, EmplaceVisitor>::visit(entity, store, copy,
                                                         registry);
    }
  }

  Prototypes() : Container<RegistryStore>(type, version) {}
};
