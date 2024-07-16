#pragma once
#include <fmt/format.h>
#include <liblog/liblog.hpp>
#define SOL_SAFE_NUMERICS 1
#include <sol/sol.hpp>

using namespace LibLog;

void injectLogger(sol::state &lua, Logger &luaLog) {
  lua.set_function("info", [&luaLog](std::string msg) { luaLog.info(msg); });
  lua.set_function("debug", [&luaLog](std::string msg) { luaLog.debug(msg); });
  lua.set_function("var", [&luaLog](std::string msg, std::string val) {
    luaLog.var(msg, val);
  });
  lua.set_function("warn", [&luaLog](std::string msg) { luaLog.warn(msg); });
  lua.set_function("error", [&luaLog](std::string msg) { luaLog.error(msg); });

  lua.set_function("start", [&luaLog](std::string msg) { luaLog.start(msg); });
  lua.set_function("stop", [&luaLog](std::string msg) { luaLog.stop(msg); });
}
