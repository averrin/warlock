#include <chrono>
#include <game/draw_engine.hpp>
#include <game/state.hpp>
#include <libcolor/libcolor.hpp>
#include <thread>
#include <utils/assets_loader.hpp>
#include <utils/entt_draw.hpp>
#include <utils/entt_lua.hpp>
#include <nlohmann/json.hpp>
#include <webview/webview.h>
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.

DrawEngine::DrawEngine(std::shared_ptr<Viewport> viewport)
    : viewport(viewport) {
}
// DrawEngine::~DrawEngine() {}

void DrawEngine::resize(sf::Vector2u size) { }

void DrawEngine::init(/*LibLog::Logger parentLog*/) {
  // log.setParent(&parentLog);
  auto label = "Initializing DrawEngine";
  log.start(label);
  started = true;

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
  auto &current_state = entt::locator<State>::value();
  transform_observer.connect(current_state.registry,
                             entt::collector.update<wl::transform>());
  text_observer.connect(current_state.registry,
                        entt::collector.update<wl::text>());
  line_observer.connect(current_state.registry,
                        entt::collector.update<wl::line>());
  sprite_observer.connect(current_state.registry,
                          entt::collector.update<wl::sprite>());
  state_observer.connect(current_state.registry,
                         entt::collector.update<wl::visual_state>());
  auto &emitter = entt::locator<event_emitter>::value();
  // drawJob =
  //     std::make_shared<Job>("Draw Job", std::bind(&DrawEngine::draw, this));
  // emitter.publish(add_job_event{drawJob, true});
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

  auto &current_state = entt::locator<State>::value();

  // TODO: change full redraw to partials
  transform_observer.each([&](const auto entity) { fullRedraw = true; });
  text_observer.each([&](const auto entity) { fullRedraw = true; });
  line_observer.each([&](const auto entity) { fullRedraw = true; });
  sprite_observer.each([&](const auto entity) { fullRedraw = true; });
  state_observer.each([&](const auto entity) { fullRedraw = true; });
  if (!fullRedraw) {
    return;
  }
  fullRedraw = false;

  ignored.clear();

  for (auto &c : current_state.registry.view<wl::visual_state>()) {
    auto state = current_state.registry.get<wl::visual_state>(c);
    if (state.hidden) {
      ignored.push_back((int)c);
      if (current_state.registry.all_of<wl::relation>(c)) {
        auto rel = current_state.registry.get<wl::relation>(c);
        for (auto &child : rel.children) {
          ignored.push_back((int)child);
        }
      }
    }
  }

  nlohmann::json state_json = {
    {"texts", nlohmann::json::array()},
    {"lines", nlohmann::json::array()},
    {"sprites", nlohmann::json::array()},
    {"hitboxes", nlohmann::json::array()}
  };

  for (auto &f : current_state.registry.view<wl::text>()) {
    if (std::find(ignored.begin(), ignored.end(), (int)f) != ignored.end()) continue;
    auto text = current_state.registry.get<wl::text>(f);
    auto transform = current_state.registry.get<wl::transform>(f);
    if (transform.relative) {
      auto parent = current_state.registry.get<wl::transform>(
          current_state.registry.get<wl::relation>(f).parent);
      transform.position.x = transform.position.x + parent.position.x;
      transform.position.y = transform.position.y + parent.position.y;
    }
    state_json["texts"].push_back({
      {"id", (int)f},
      {"text", text.text},
      {"x", transform.position.x},
      {"y", transform.position.y},
      {"color", {text.color.r, text.color.g, text.color.b, text.color.a}},
      {"size", text.size},
      {"layer", transform.layer}
    });
  }

  for (auto &c : current_state.registry.view<wl::line>()) {
    if (std::find(ignored.begin(), ignored.end(), (int)c) != ignored.end()) continue;
    auto l = current_state.registry.get<wl::line>(c);
    state_json["lines"].push_back({
      {"id", (int)c},
      {"x1", l.position1.x},
      {"y1", l.position1.y},
      {"x2", l.position2.x},
      {"y2", l.position2.y},
      {"thickness", l.thickness},
      {"color", {l.color.r, l.color.g, l.color.b, l.color.a}},
      {"layer", l.layer}
    });
  }

  for (auto &c : current_state.registry.view<wl::sprite>()) {
    if (std::find(ignored.begin(), ignored.end(), (int)c) != ignored.end()) continue;
    auto s = current_state.registry.get<wl::sprite>(c);
    auto transform = current_state.registry.get<wl::transform>(c);
    if (transform.relative) {
      auto parent = current_state.registry.get<wl::transform>(
          current_state.registry.get<wl::relation>(c).parent);
      transform.position.x = transform.position.x + parent.position.x;
      transform.position.y = transform.position.y + parent.position.y;
    }

    nlohmann::json visual_state = nlohmann::json::object();
    if (current_state.registry.all_of<wl::visual_state>(c)) {
      auto state = current_state.registry.get<wl::visual_state>(c);
      visual_state["ghost"] = state.ghost;
      visual_state["selected"] = state.selected;
      visual_state["hovered"] = state.hovered;
    }

    state_json["sprites"].push_back({
      {"id", (int)c},
      {"sprite", s.sprite},
      {"x", transform.position.x},
      {"y", transform.position.y},
      {"width", s.rect.width},
      {"height", s.rect.height},
      {"scale", transform.scale},
      {"rotation", transform.rotation},
      {"layer", transform.layer},
      {"state", visual_state}
    });
  }

  for (auto &c : current_state.registry.view<wl::rect>()) {
    if (std::find(ignored.begin(), ignored.end(), (int)c) != ignored.end()) continue;
    auto transform = current_state.registry.get<wl::transform>(c);
    auto r = current_state.registry.get<wl::rect>(c);

    if (transform.relative) {
      auto parent = current_state.registry.get<wl::transform>(
          current_state.registry.get<wl::relation>(c).parent);
      transform.position.x = transform.position.x + parent.position.x;
      transform.position.y = transform.position.y + parent.position.y;
    }

    state_json["hitboxes"].push_back({
      {"id", (int)c},
      {"x", transform.position.x},
      {"y", transform.position.y},
      {"width", r.width},
      {"height", r.height},
      {"layer", transform.layer}
    });
  }

  auto w_handle = entt::monostate<"webview"_hs>{};
  if ((void*)w_handle != nullptr) {
    std::string js = fmt::format("if (window.updateState) window.updateState({});", state_json.dump());
    webview_eval(w_handle, js.c_str());
  }
}
