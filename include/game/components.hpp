#pragma once
#include <utils/entt.hpp>
using namespace entt::literals;
#include <game/components/frame.hpp>
#include <game/components/resource_patch.hpp>
#include <iostream>
#include <utils/entt_draw.hpp>
#include <utils/entt_lua.hpp>

template <typename... Types> struct ComponentList {};

using all_components =
    ComponentList<hf::meta, hf::ineditor,
                  hf::tags, hf::player,
                  hf::obstacle, hf::creature, hf::script,
                  entt::tag<"proto"_hs>, Frame,
                  Connection, Environment, SpendablePool, wl::transform,
                  wl::relation, ResourcePatch>;

template <typename ComponentList, template <typename> class Visitor,
          std::size_t Index = 0>
struct TypeVisitor;

template <template <typename> class Visitor, typename T, typename... Rest,
          std::size_t Index>
struct TypeVisitor<ComponentList<T, Rest...>, Visitor, Index> {
  static void visit(entt::entity entity, entt::registry &src, entt::entity copy,
                    entt::registry &dest) {
    Visitor<T>::visit(entity, src, copy, dest, Index);
    TypeVisitor<ComponentList<Rest...>, Visitor, Index + 1>::visit(entity, src,
                                                                   copy, dest);
  }
};

template <template <typename> class Visitor, std::size_t Index>
struct TypeVisitor<ComponentList<>, Visitor, Index> {
  static void visit(entt::entity entity, entt::registry &src, entt::entity copy,
                    entt::registry &dest) {}
};

template <typename T> struct EmplaceVisitor {
  static void visit(entt::entity entity, entt::registry &src, entt::entity copy,
                    entt::registry &dest, std::size_t index) {
    if (src.all_of<T>(entity)) {
      dest.storage<T>().push(copy, src.storage<T>().value(entity));
    }
  }
};
