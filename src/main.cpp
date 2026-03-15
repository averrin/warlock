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
#include <webview/webview.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
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
  // if (argc > 1) {
  //   seed = std::atoi(argv[1]);
  // }
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
  // std::shared_ptr<DrawEngine> alt_engine = nullptr;
  if (!nogui) {
    scene.init(app.log);
    draw_manager.init(app.log);
    main_engine = draw_manager.addEngine("main");
    main_engine->resize(scene.window->getSize());
  }

  // Force GTK to use X11 backend to match SFML's X11 window
  setenv("GDK_BACKEND", "x11", 1);

  app.w_handle = webview_create(1, nullptr);
  entt::monostate<"webview"_hs>{} = app.w_handle;
  webview_navigate(app.w_handle, "http://localhost:5173");

  // Embed the webview window inside the SFML window as a background
  if (!nogui) {
    auto sfml_size = scene.window->getSize();

    // Get the GTK window and configure it before showing
    GtkWidget *gtk_win = static_cast<GtkWidget *>(webview_get_window(app.w_handle));
    gtk_window_set_decorated(GTK_WINDOW(gtk_win), FALSE);

    // Set webview size (triggers window_show internally)
    webview_set_size(app.w_handle, sfml_size.x, sfml_size.y, WEBVIEW_HINT_FIXED);

    // Show and realize the GTK window
    gtk_widget_show_all(gtk_win);
    gtk_widget_realize(gtk_win);

    // Pump GTK events until the window is fully mapped
    for (int i = 0; i < 100; i++) {
      g_main_context_iteration(nullptr, FALSE);
    }

    GdkWindow *gdk_win = gtk_widget_get_window(gtk_win);
    if (gdk_win && GDK_IS_X11_WINDOW(gdk_win)) {
      Display *x_display = gdk_x11_display_get_xdisplay(gdk_display_get_default());
      Window wv_xid = gdk_x11_window_get_xid(gdk_win);
      Window sfml_xid = scene.window->getSystemHandle();

      // Reparent the webview's X11 window into the SFML window
      XReparentWindow(x_display, wv_xid, sfml_xid, 0, 0);
      XMoveResizeWindow(x_display, wv_xid, 0, 0, sfml_size.x, sfml_size.y);
      XLowerWindow(x_display, wv_xid);
      XMapWindow(x_display, wv_xid);
      XFlush(x_display);
    } else {
      fmt::print("WARNING: GDK window is not X11, webview embedding skipped\n");
    }
  }

  auto &gui = entt::locator<Gui>::emplace();
  if (!noeditor) {
    gui.init(app.log);
  }

  auto &loader = entt::locator<Loader>::emplace();
  loader.init(app.log);

  auto &gm = entt::locator<GameManager>::emplace();
  // auto gm = GameManager();
  gm.init(app.log);

  // emitter.publish(add_job_event{gm.startJob, true});
  jobs.add(gm.startJob, true);

  if (!noeditor) {
    gui.renders.push_back([&]() {
      ImGui::ShowDemoWindow();
      ImPlot::ShowDemoWindow();
    });
    // auto md_editor = std::make_shared<MetaDataEditor>("Meta Data Editor");
    auto md_editor = MetaDataEditor("Meta Data Editor");
    gui.renders.push_back([&]() { md_editor.render(); });
    auto et_editor = EntityTreeEditor("Prototype Editor");
    gui.renders.push_back([&]() {
      et_editor.render<Prototypes, entt::tag<"proto"_hs> /*, hf::meta*/>(
          entt::locator<Prototypes>::value());
    });
    auto state_editor = StateEditor("State Editor");
    jobs.add(state_editor.startJob, true);
    // emitter.publish(add_job_event{state_editor.startJob, true});
    gui.renders.push_back([&]() { state_editor.render(); });
    // auto ts_editor = TilesetEditor("Tileset Editor");
    // gui.renders.push_back([&]() { ts_editor.render(); });

    auto ide = std::make_shared<IDE>();
    ide->init(path);
    gui.renders.push_back([=]() { ide->render(); });

    auto power_editor = std::make_shared<PowerEditor>("Power Editor");
    gui.editors.push_back(power_editor);

    auto game_editor = std::make_shared<GameEditor>("Game Editor");
    gui.editors.push_back(game_editor);
    /*
    gui.renders.push_back([&]() {
      // if (gm.started) {
        game_editor->render();
      // }
    });
    */

    /*
    sf::RenderTexture rt;
    sf::Sprite s;
    gui.renders.push_back([&]() {
      ImGui::Begin("Test window");
      // rt.draw(*alt_engine->layers);
      rt.display();
      s.setTexture(rt.getTexture());
      ImGui::Image(s, sf::Vector2f(800, 600), sf::Color::White,
                   sf::Color::Transparent);
      ImGui::End();
    });
    */
  }

  if (!nogui) {
    sf::RectangleShape rectangle;
    rectangle.setPosition(0, 0);
    rectangle.setSize(
        sf::Vector2f(scene.window->getSize().x, scene.window->getSize().y));

    main_engine->start();
    // alt_engine->start();
    // fmt::print("Main Engine: {}\n", fmt::ptr(main_engine.get()));
  }

  while (nogui || scene.window->isOpen()) {
    // Pump GTK/WebKit events non-blockingly on the main thread
    while (g_main_context_iteration(nullptr, FALSE)) {}

    app.serve();
    gm.serve();
    if (!nogui) {
      scene.serve();
      // draw_manager.serve();
      // draw_manager.draw();
      if (gm.started) {
        main_engine->_draw();
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
