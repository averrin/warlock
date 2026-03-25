#pragma once
#include <game/prototypes.hpp>
#include <utils/entt.hpp>

class EnttTools {
public:
  static entt::entity createEntityFromPrototype(std::string protoID,
                                                entt::registry &reg,
                                                std::string name = "") {
    auto &prototypes = entt::locator<Prototypes>::value();
    for (auto entity : prototypes.registry.view<hf::meta>()) {
      auto &meta = prototypes.registry.get<hf::meta>(entity);
      if (meta.id == protoID) {
        auto copy = copyEntity(entity, prototypes.registry, reg);
        if (name != "") {
          meta.name = name;
          reg.emplace_or_replace<hf::meta>(copy, meta);
        }
        if (reg.all_of<wl::relation>(copy)) {
          auto &relation = reg.get<wl::relation>(copy);
          for (auto &child : relation.children) {
            auto child_copy = createEntityFromPrototype(
                prototypes.registry.get<hf::meta>(child).id, reg);
            relation.children.push_back(child_copy);
          }
        }
        return copy;
      }
    }
    throw std::runtime_error("Prototype not found");
  }

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
};
