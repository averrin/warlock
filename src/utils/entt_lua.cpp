#include <utils/entt_lua.hpp>
#include <utils/jobs.hpp>

void initEnttLua() {
  auto &lua = entt::locator<sol::state>::value();
  auto &emitter = entt::locator<event_emitter>::value();

  auto lua_emitter = lua["emitter"].get_or_create<sol::table>();
  lua_emitter.set_function("publish", [&emitter](std::string event, sol::table args) {
    lua_event e;
    e.event = event;
    e.args = args;
    emitter.publish(e);
  });
  lua_emitter.set_function("connect", [&emitter](std::string event, sol::function f) {
    emitter.connect<lua_event>([=](const lua_event &e, const auto &emitter) {
      if (e.event == event) {
        f(e.args);
      }
    });
  });

  emitter.connect<init_event>([&](const auto &e, const auto &em) {
    auto &lua = entt::locator<sol::state>::value();
    auto args = lua.create_table();
    args["component"] = e.component;
    emitter.publish(lua_event{"init", args});
  });

  emitter.connect<key_event>([&](const auto &e, const auto &em) {
    auto &lua = entt::locator<sol::state>::value();
    auto args = lua.create_table();
    args["combo"] = e.combo;
    args["key"] = e.key;
    args["sys"] = e.sys;
    args["alt"] = e.alt;
    args["control"] = e.control;
    args["shift"] = e.shift;
    emitter.publish(lua_event{"key", args});
  });

}
