#pragma once
#include <filesystem>
#include <string>
namespace fs = std::filesystem;

#include <liblog/liblog.hpp>

class Application {
public:
  std::string APP_NAME;
  std::string VERSION;
  fs::path PATH;
  std::string PATH_STR;

  LibLog::Logger &log = LibLog::Logger::getInstance();
  LibLog::Logger luaLog = LibLog::Logger(fmt::color::aqua, "LUA");

  Application(std::string, fs::path, std::string, int);
  ~Application();

  void initLua();
  void initConfig();

  int serve();
};
