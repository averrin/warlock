#include <app/application.hpp>
#include <backward.hpp>
#include <chrono>
#include <fmt/format.h>
#include <meta.hpp>
#include <utils/data/loader.hpp>
#include <utils/job_manager.hpp>
#include <utils/selfpath.hpp>
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.

#include <app/gui.hpp>
#include <app/scene.hpp>
#include <game/game_manager.hpp>
#include <utils/entt.hpp>

#include <IconsFontAwesome6.h>
#include <argparse/argparse.hpp>
#include <imgui.h>

#include <editors/entity_tree_editor.hpp>
#include <editors/meta_data_editor.hpp>
#include <editors/state_editor.hpp>
#include <editors/tileset_editor.hpp>
#include <game/prototypes.hpp>
#include <game/viewport.hpp>

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
  bool noeditor = program["--no-editor"] == true;

  bool nostate = program["--new"] == true;

  auto seed = time(NULL);
  // if (argc > 1) {
  //   seed = std::atoi(argv[1]);
  // }
  auto path = get_selfpath();
  entt::monostate<"path"_hs>{} = path;

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

  auto &scene = entt::locator<Scene>::emplace();
  auto &viewport = entt::locator<Viewport>::emplace();

  viewport.init(app.log);
  emitter.publish(add_job_event{viewport.startJob, true});
  // viewport.start();

  if (!nogui) {
    scene.init(app.log);
  }

  auto &gui = entt::locator<Gui>::emplace();
  if (!nogui && !noeditor) {
    gui.init(app.log);
  }

  auto &loader = entt::locator<Loader>::emplace();
  loader.init(app.log);

  auto &gm = entt::locator<GameManager>::emplace();
  gm.init(app.log);

  emitter.publish(add_job_event{gm.startJob, true});

  if (!noeditor) {
    gui.renders.push_back([&]() { ImGui::ShowDemoWindow(); });
    // auto md_editor = std::make_shared<MetaDataEditor>("Meta Data Editor");
    auto md_editor = MetaDataEditor("Meta Data Editor");
    gui.renders.push_back([&]() { md_editor.render(); });
    auto et_editor = EntityTreeEditor("Prototype Editor");
    gui.renders.push_back([&]() {
      et_editor.render<Prototypes, entt::tag<"proto"_hs>>(
          entt::locator<Prototypes>::value());
    });
    auto state_editor = std::make_shared<StateEditor>("State Editor");
    jobs.add(state_editor->startJob, true);
    // emitter.publish(add_job_event{state_editor->startJob, true});
    gui.renders.push_back([&]() { state_editor->render(); });
    auto ts_editor = TilesetEditor("Tileset Editor");
    gui.renders.push_back([&]() { ts_editor.render(); });

    // gui.renders.push_back([&]() {
    //   ImGui::Begin("Test window");
    //   ImGui::Text("\xef\x8a\xb9");
    //   ImGui::Text("Hellfrost");
    //   ImGui::End();
    // });
  }

  while (nogui || scene.window->isOpen()) {
    app.serve();
    gm.serve();
    if (!nogui) {
      scene.serve();
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
