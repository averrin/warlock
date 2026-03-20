#include <fmt/format.h>
#include <game/draw_engine.hpp>
#include <game/viewport.hpp>
#include <liblog/liblog.hpp>

class DrawManager {
  LibLog::Logger log = LibLog::Logger(fmt::color::red, "DM");

public:
  std::map<std::string, std::shared_ptr<DrawEngine>> engines;

  void init(LibLog::Logger &parentLog);
  void start();
  void serve();

  std::shared_ptr<DrawEngine> addEngine(std::string name);
  // Default constructor
  DrawManager() {}
  // Copy constructor
  DrawManager(const DrawManager &other) = default;
};
