#include <game/draw_manager.hpp>

void DrawManager::init(LibLog::Logger &parentLog) {
  log.setParent(&parentLog);
  auto label = "Initializing GameManager";
  log.start(label);
  log.stop(label);
}

std::shared_ptr<DrawEngine> DrawManager::addEngine(std::string name) {
  auto &emitter = entt::locator<event_emitter>::value();
  auto viewport = std::make_shared<Viewport>();
  viewport->init(log);
  emitter.publish(add_job_event{viewport->startJob, true});
  // viewport.start();
  auto engine = std::make_shared<DrawEngine>(viewport);
  engine->init();
  engines[name] = engine;
  return engine;
}

void DrawManager::serve() {}
