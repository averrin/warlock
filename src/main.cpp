#include <app/application.hpp>
#include <backward.hpp>
#include <chrono>
#include <fmt/format.h>
#include <imgui-SFML.h>
#include <imgui-stl.hpp>
#include <implot.h>
#include <meta.hpp>
#include <misc/cpp/imgui_stdlib.h>
#include <utils/data/loader.hpp>
#include <utils/job_manager.hpp>
#include <utils/selfpath.hpp>
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.

#include <app/gui.hpp>
#include <app/ide.hpp>
#include <app/scene.hpp>
#include <game/draw_engine.hpp>
#include <game/draw_manager.hpp>
#include <game/game_manager.hpp>
#include <utils/entt.hpp>

#include <IconsFontAwesome6.h>
#include <argparse/argparse.hpp>
#include <imgui.h>

#include <editors/entity_tree_editor.hpp>
#include <editors/game_editor.hpp>
#include <editors/meta_data_editor.hpp>
#include <editors/power_editor.hpp>
#include <editors/state_editor.hpp>
#include <editors/tileset_editor.hpp>
#include <game/prototypes.hpp>

namespace backward {
backward::SignalHandling sh;
} // namespace backward

int main(int argc, char *argv[]) {

  argparse::ArgumentParser program(APP_NAME);

  program.add_argument("--no-gui")
      .help("disable gui")
      .default_value(false)
      .implicit_value(true);

  program.add_argument("--no-editor")
      .help("disable editor")
      .default_value(false)
      .implicit_value(true);

  program.add_argument("--no-debug")
      .help("disable debug logs")
      .default_value(false)
      .implicit_value(true);

  program.add_argument("--new")
      .help("remove current save")
      .default_value(false)
      .implicit_value(true);

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }
  bool nogui = program["--no-gui"] == true;
  bool noeditor = nogui || program["--no-editor"] == true;

  bool nostate = program["--new"] == true;
  bool nodebug = program["--no-debug"] == true;

  auto seed = time(NULL);
  auto path = get_selfpath();
  entt::monostate<"id"_hs>{} = -1;
  entt::monostate<"path"_hs>{} = path;
  entt::monostate<"debug"_hs>{} = !nodebug;

  entt::locator<event_emitter>::emplace();
  Application app(APP_NAME, path, VERSION, seed);
  app.log.start(APP_NAME);
  auto &emitter = entt::locator<event_emitter>::value();

  if (nostate) {
    auto &lua = entt::locator<sol::state>::value();
    fs::path PATH = entt::monostate<"path"_hs>{};
    auto state_path =
        PATH / fs::path(lua["settings"]["current_state"].get<std::string>());
    app.log.info("Removing state: {}", state_path.string());
    fs::remove(state_path);
  }

  auto &jobs = entt::locator<JobManager>::emplace();
  jobs.init(app.log);
  jobs.sync = true;

  auto &scene = entt::locator<Scene>::emplace();
  auto &draw_manager = entt::locator<DrawManager>::emplace();

  std::shared_ptr<DrawEngine> main_engine = nullptr;
  if (!nogui) {
    scene.init(app.log);
    draw_manager.init(app.log);
    main_engine = draw_manager.addEngine("main");
    main_engine->resize(scene.window->getSize());
  }

  auto &gui = entt::locator<Gui>::emplace();
  if (!noeditor) {
    gui.init(app.log);
  }

  auto &loader = entt::locator<Loader>::emplace();
  loader.init(app.log);

  auto &gm = entt::locator<GameManager>::emplace();
  gm.init(app.log);
  jobs.add(gm.startJob, true);

  if (!noeditor) {
    gui.renders.push_back([&]() {
      ImGui::ShowDemoWindow();
      ImPlot::ShowDemoWindow();
    });
    auto md_editor = MetaDataEditor("Meta Data Editor");
    gui.renders.push_back([&]() { md_editor.render(); });
    auto et_editor = EntityTreeEditor("Prototype Editor");
    gui.renders.push_back([&]() {
      et_editor.render<Prototypes, entt::tag<"proto"_hs> /*, hf::meta*/>(
          entt::locator<Prototypes>::value());
    });
    auto state_editor = StateEditor("State Editor");
    jobs.add(state_editor.startJob, true);
    gui.renders.push_back([&]() { state_editor.render(); });

    auto ide = std::make_shared<IDE>();
    ide->init(path);
    gui.renders.push_back([=]() { ide->render(); });

    auto power_editor = std::make_shared<PowerEditor>("Power Editor");
    gui.editors.push_back(power_editor);

    auto game_editor = std::make_shared<GameEditor>("Game Editor");
    gui.editors.push_back(game_editor);
  }

  if (!nogui) {
    sf::RectangleShape rectangle;
    rectangle.setPosition(0, 0);
    rectangle.setSize(
        sf::Vector2f(scene.window->getSize().x, scene.window->getSize().y));

    main_engine->start();
  }

  while (nogui || scene.window->isOpen()) {
    app.serve();
    gm.serve();
    if (!nogui) {
      scene.serve();
      if (gm.started) {
        main_engine->_draw();
        scene.window->draw(main_engine->cache);
      }
      if (!noeditor) {
        gui.serve();
      }
      scene.draw();
    } else {
      std::this_thread::sleep_for(50ms);
    }
  }
  app.log.stop(APP_NAME);
  return EXIT_SUCCESS;
}
