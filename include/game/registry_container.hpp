#pragma once
#include <game/registry_store.hpp>
#include <iostream>
#include <liblog/liblog.hpp>
#include <utils/data/container.hpp>
#include <utils/entt.hpp>

class RegistryContainer : public Container<RegistryStore> {
  LibLog::Logger log = LibLog::Logger(fmt::color::orange, "RegC");

  LibLog::Logger &getLog() override { return log; }
  int8_t getVersion() override { return version; }
  int8_t getType() override { return type; }

public:
  int8_t version = 3;
  int8_t type = 2;

  entt::registry registry;

  static void copyRegistry(entt::registry &src, entt::registry &dst) {
    for (auto entity : src.view<hf::meta>()) {
      // fmt::print("Copying entity: {}\n", (int)entity);
      auto existing = dst.view<hf::meta>();
      auto copy = dst.create();
      for (auto e : existing) {
        if (dst.get<hf::meta>(e).id == src.get<hf::meta>(entity).id) {
          dst.destroy(e);
          if (!dst.all_of<entt::tag<"override"_hs>>(copy)) {
            dst.emplace<entt::tag<"override"_hs>>(copy);
          }
          if (!src.all_of<entt::tag<"override"_hs>>(entity)) {
            src.emplace<entt::tag<"override"_hs>>(entity);
          }
        }
      }
      TypeVisitor<all_components, EmplaceVisitor>::visit(entity, src, copy,
                                                         dst);
    }
  }

  static entt::entity copyEntity(entt::entity entity, entt::registry &src,
                                 entt::registry &dst) {
    auto copy = dst.create();
    TypeVisitor<all_components, EmplaceVisitor>::visit(entity, src, copy, dst);
    return copy;
  }

  void combine(std::shared_ptr<RegistryStore> store) override {
    RegistryContainer::copyRegistry(store->registry, registry);
  }

  RegistryContainer() : Container<RegistryStore>(getType(), getVersion()) {
    log.setAsync(true);
  }
  RegistryContainer(const RegistryContainer &other)
      : Container<RegistryStore>(getType(), getVersion()) {
    log.setAsync(true);
    for (auto store : other.stores) {
      add(store);
    }
  }

  void clear() {
    fmt::print("Clearing registry\n");
    registry.clear();
    // Container<RegistryStore>::clear();
    for (auto store : stores) {
      fmt::print("Clearing store: {}\n", store->name);
      store->registry.clear();
    }
  }
};
