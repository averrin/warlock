#include <utils/entt.hpp>
using namespace entt::literals;
#include <specs/light.hpp>
#include <utils/entt_lua.hpp>
#include <iostream>

template <typename... Types> struct ComponentList {};

using all_components =
    ComponentList<hf::meta, hf::visible, hf::ineditor,
                  // hf::pickable, hf::wearable,
                  hf::glow, hf::renderable, hf::wall, hf::tags, hf::player,
                  hf::vision, hf::obstacle, hf::creature, hf::script,
                  entt::tag<"item"_hs>, entt::tag<"proto"_hs>>;


