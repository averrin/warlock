#include <chrono>
#include <game/draw_engine.hpp>
#include <game/state.hpp>
#include <libcolor/libcolor.hpp>
#include <thread>
#include <utils/entt_lua.hpp>
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.

DrawEngine::DrawEngine(std::shared_ptr<Viewport> viewport)
    : viewport(viewport) {}
// DrawEngine::~DrawEngine() {}

void DrawEngine::resize(sf::Vector2u size) { layers->resize(size); }

void DrawEngine::init(/*LibLog::Logger parentLog*/) {
  // log.setParent(&parentLog);
  auto label = "Initializing DrawEngine";
  log.start(label);
  started = false;

  layers = std::make_shared<LayersManager>();
  auto &lua = entt::locator<sol::state>::value();

  auto zIndex = 0;
  for (auto l : lua["draw"]["layers"].get<std::vector<sol::table>>()) {
    auto n = l["name"].get<std::string_view>();
    layers->layers[n] = std::make_shared<Layer>();
    layers->layers[n]->zIndex = zIndex;
    auto e = l["enabled"].get_or(true);
    layers->layers[n]->enabled = e;
    if (e) {
      log.var("Layer", n);
    }

    zIndex++;
  }

  auto &emitter = entt::locator<event_emitter>::value();
  emitter.connect<ready_event>([&](const auto &e, const auto &em) {
    if (e.component == "viewport") {
      auto c = LibColor::Color::fromHexString(
          viewport->colors["PALETTE"]["BACKGROUND"]);
      bgColor = sf::Color(c.r, c.g, c.b, c.a);
      started = true;
    }
  });

  log.stop(label);
}

void DrawEngine::serve() {}
void DrawEngine::start() {
  auto &lua = entt::locator<sol::state>::value();
  for (auto &l : layers->layers) {
    fmt::print("2 Layer: {}\n", l.first);
  }

  auto settings = lua["gui"];
  auto main_font = settings["font"].get<std::string>();
  if (!font.loadFromFile(main_font)) {
    log.error("No font");
  }
  text = std::make_shared<sf::Text>();
  text->setFont(font); // font is a sf::Font
  text->setPosition(800, 800);

  auto &emitter = entt::locator<event_emitter>::value();
  drawJob =
      std::make_shared<Job>("Draw Job", std::bind(&DrawEngine::draw, this));
  emitter.publish(add_job_event{drawJob, true});
}

void DrawEngine::draw() {
  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    count++;
    _draw();
    // fmt::print("DrawEngine {}: {}\n", fmt::ptr(this), count);
  }
}

void DrawEngine::_draw() {
  if (!started) {
    log.error("DrawEngine not started");
    return;
  }

  std::lock_guard<std::mutex> guard(renderMutex);

  text->setString(fmt::format("Main DrawEngine: {}", count).c_str());
  text->setCharacterSize(24);
  text->setFillColor(sf::Color::Red);
  layers->layers["bottom"]->draw(text, 0);
}
