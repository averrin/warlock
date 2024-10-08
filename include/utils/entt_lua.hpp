#pragma once
#include <string>
#include <utils/entt.hpp>
#define SOL_SAFE_NUMERICS 1
#include <sol/sol.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>

struct lua_event {
  std::string event;
  sol::table args;
};

void initEnttLua();
void register_bindings(sol::state& lua);

namespace hf {
struct script {
  std::string path = "";

  sol::table self;
  struct {
    sol::function init;
    sol::function create;
    sol::function update;
    sol::function destroy;
    sol::function interact;
  } handlers;
  bool enabled = true;

  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const { ar(path, enabled); };
  template <class Archive> void load(Archive &ar) { ar(path, enabled); };
};
} // namespace hf
