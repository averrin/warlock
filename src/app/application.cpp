#include <app/application.hpp>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <filesystem>
namespace fs = std::filesystem;
#define SOL_SAFE_NUMERICS 1
#include <sol/sol.hpp>
#include <lua/logger.hpp>

Application::Application(std::string app_name, fs::path path,
                         std::string version, int s)
    : APP_NAME(app_name), VERSION(version), PATH(path), PATH_STR(path.string()) {
  fmt::print("Path: {}\n", PATH.string());

  auto label = "Init Application";
  log.start(label);

  initLua();
  initEnttLua();
  initConfig();

  auto &emitter = entt::locator<event_emitter>::value();
  emitter.connect<init_event>([&](const auto &e, const auto &em) {
    // log.debug("Init event app handler: {}", e.component);
  });

  emitter.publish(init_event{"app"});
  log.stop(label);
}

void Application::initConfig() {
  auto &lua = entt::locator<sol::state>::value();

  auto cp = fs::path(PATH / "scripts" / "config.lua");
  if (!fs::exists(cp)) {
    log.error("config.lua doesn't exist!");
    exit(111);
  }
  sol::table settings = lua.script_file(cp);
  entt::monostate<"settings"_hs>{} = settings;
}

void Application::initLua() {
  entt::locator<sol::state>::emplace();

  auto &lua = entt::locator<sol::state>::value();
  lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string,
                     sol::lib::table, sol::lib::math, sol::lib::os);
  injectLogger(lua, luaLog);
  luaLog.setParent(&log);

  lua.new_usertype<Application>("Application", "new", sol::no_constructor,
                                "APP_NAME", &Application::APP_NAME,
                                "VERSION", &Application::VERSION,
                                "PATH", &Application::PATH_STR
                                );
  lua.set("app", this);
  lua.set_function("exit", [&]() { 
    auto &emitter = entt::locator<event_emitter>::value();
    emitter.publish(close_event{"Lua call"});
  });
}

int Application::serve() {
  return 0;
}

Application::~Application() {
  auto &lua = entt::locator<sol::state>::value();
  lua.collect_garbage();
  lua.stack_clear();
}
