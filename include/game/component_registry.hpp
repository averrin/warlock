#pragma once
// ---------------------------------------------------------------------------
// Component Registry — single source of truth for component type <-> name.
//
// To add a new component:
//   1. Add a line: X(MyComponent, "MyComponent")
//   2. Add field_save/field_load to the component struct
//   That's it. Serialization, deserialization, and entity copying all update
//   automatically. No version bumps or migration code needed for field changes.
//
// To remove a component:
//   1. Remove the X(...) line here.
//   Old files containing that component will silently skip its block.
// ---------------------------------------------------------------------------

// Forward declarations needed so this header can be included before full defs.
// (Full definitions are included by registry_store.hpp which includes this.)

// clang-format off
#define COMPONENT_LIST(X)                              \
  X(hf::meta,                  "meta")                \
  X(hf::visible,               "visible")             \
  X(hf::ineditor,              "ineditor")            \
  X(hf::glow,                  "glow")                \
  X(hf::renderable,            "renderable")          \
  X(hf::wall,                  "wall")                \
  X(hf::tags,                  "tags")                \
  X(hf::player,                "player")              \
  X(hf::vision,                "vision")              \
  X(hf::obstacle,              "obstacle")            \
  X(hf::creature,              "creature")            \
  X(hf::script,                "script")              \
  X(Frame,                     "Frame")               \
  X(Connection,                "Connection")          \
  X(Environment,               "Environment")         \
  X(wl::transform,             "transform")           \
  X(wl::sprite,                "sprite")              \
  X(wl::relation,              "relation")            \
  X(wl::text,                  "text")                \
  X(entt::tag<"proto"_hs>,     "proto")               \
  X(entt::tag<"item"_hs>,      "item")
// clang-format on
