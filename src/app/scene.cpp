#include <app/scene.hpp>
#include <utils/entt_lua.hpp>

Scene::Scene() {}

Scene::~Scene() {}

void Scene::init(LibLog::Logger parentLog) {
  log.setParent(&parentLog);
  auto label = "Initializing Scene";
  log.start(label);
  auto &emitter = entt::locator<event_emitter>::value();

  emitter.connect<close_event>(
      [&](close_event &event, const event_emitter &emitter) {
        log.setParent(nullptr);
        log.warn("Close event received: {}", event.reason);
        window->close();
      });

  auto &lua = entt::locator<sol::state>::value();
  controls = std::make_shared<Controls>();

  lua.new_usertype<sf::RenderWindow>("window", "new", sol::no_constructor,
                                     "close", &sf::RenderWindow::close);

  lua.new_usertype<Scene>("scene", "new", sol::no_constructor, "window",
                          &Scene::window);

  sf::ContextSettings settings;
  settings.antialiasingLevel = 8;
  // settings.antialiasingLevel = 4;
  auto name = lua["app"]["APP_NAME"].get<std::string>();
  auto mode = sf::VideoMode::getDesktopMode();
  log.debug("Desktop mode: {}x{}", mode.width, mode.height);
  window = new sf::RenderWindow(mode, name, sf::Style::Default, settings);

  log.debug("Window created: {}", window->getSize().x);
  window->setVerticalSyncEnabled(true);

  window->resetGLStates();
  window->clear(sf::Color::Black);

  emitter.publish(init_event{"scene"});
  log.stop(label);
}

void Scene::serve() { window->clear(sf::Color::Black); }

void Scene::draw() {
  // log.start("serve");

  sf::Event event;
  while (window->pollEvent(event)) {
    processEvent(event);
  }
  window->display();

  // log.stop("serve");
}

void Scene::processEvent(sf::Event event) {
  auto &emitter = entt::locator<event_emitter>::value();
  emitter.publish(sf_event_event{event});
  switch (event.type) {
  case sf::Event::Closed:
    emitter.publish(close_event{"Window closed"});
    break;
  case sf::Event::KeyPressed:
    if (acceptInput)
      controls->processEvent(event);
    break;
  default:
    break;
  }
}
