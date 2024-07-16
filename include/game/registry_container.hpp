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
  int8_t version = 1;
  int8_t type = 2;

  entt::registry registry;

  static void copyRegistry(entt::registry &src, entt::registry &dst) {
    for (auto entity : src.view<hf::meta>()) {
      auto existing = dst.view<hf::meta>();
      auto copy = dst.create();
      for (auto e : existing) {
        if (dst.get<hf::meta>(e).id == src.get<hf::meta>(entity).id) {
          dst.destroy(e);
          dst.emplace<entt::tag<"override"_hs>>(copy);
          src.emplace<entt::tag<"override"_hs>>(entity);
        }
      }
      TypeVisitor<all_components, EmplaceVisitor>::visit(entity, src, copy,
                                                         dst);
    }
  }

  void combine(std::shared_ptr<RegistryStore> store) override {
    RegistryContainer::copyRegistry(store->registry, registry);
    // for (auto entity : store->registry.view<hf::meta>()) {
    //   auto existing = registry.view<hf::meta>();
    //   auto copy = registry.create();
    //   for (auto e : existing) {
    //     if (registry.get<hf::meta>(e).id ==
    //         store->registry.get<hf::meta>(entity).id) {
    //       registry.destroy(e);
    //       registry.emplace<entt::tag<"override"_hs>>(copy);
    //       store->registry.emplace<entt::tag<"override"_hs>>(entity);
    //       getLog().debug("Override: {} with {} [id: {}, path: {}]", (int)e,
    //                      (int)copy, store->registry.get<hf::meta>(entity).id,
    //                      store->name);
    //     }
    //   }
    //   TypeVisitor<all_components, EmplaceVisitor>::visit(
    //       entity, store->registry, copy, registry);
    // }
  }

  RegistryContainer() : Container<RegistryStore>(getType(), getVersion()) {
    log.setAsync(true);
  }
};
