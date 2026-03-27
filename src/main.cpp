#include <rpc/server.hpp>
#include <rpc/handlers/session_handler.hpp>
#include <rpc/handlers/game_handler.hpp>
#include <rpc/handlers/frame_handler.hpp>
#include <rpc/handlers/state_handler.hpp>
#include <rpc/handlers/component_handler.hpp>
#include <rpc/handlers/connection_handler.hpp>
#include <rpc/handlers/code_handler.hpp>
#include <rpc/handlers/items_handler.hpp>
#include <rpc/handlers/storage_handler.hpp>
#include <rpc/handlers/patch_handler.hpp>
#include <rpc/event_bridge.hpp>
#include <app/application.hpp>
#include <backward.hpp>
#include <chrono>
#include <thread>
#include <ixwebsocket/IXNetSystem.h>
#include <fmt/format.h>
#include <meta.hpp>
#include <utils/data/loader.hpp>
#include <utils/job_manager.hpp>
#include <utils/selfpath.hpp>
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.

#include <game/game_manager.hpp>
#include <utils/entt.hpp>

#include <argparse/argparse.hpp>

namespace backward {
backward::SignalHandling sh;
} // namespace backward

int main(int argc, char *argv[]) {
  ix::initNetSystem();

  argparse::ArgumentParser program(APP_NAME);

  program.add_argument("--ui-mode")
      .help("Headless runtime: web (default) or headless. Connect the web UI via WebSocket RPC.")
      .default_value(std::string("web"));

  program.add_argument("--no-debug")
      .help("disable debug logs")
      .default_value(false)
      .implicit_value(true);

  program.add_argument("--new")
      .help("remove current save")
      .default_value(false)
      .implicit_value(true);

  program.add_argument("--rpc-port")
      .help("WebSocket RPC server port")
      .default_value(9800)
      .scan<'i', int>();

  try {
    program.parse_args(argc, argv);
  } catch (const std::exception &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << program;
    std::exit(1);
  }
  std::string uiMode = program.get<std::string>("--ui-mode");
  if (uiMode == "native") {
    fmt::print(stderr,
               "Error: --ui-mode native was removed. Use web (default) or headless.\n");
    return EXIT_FAILURE;
  }
  if (uiMode != "web" && uiMode != "headless") {
    fmt::print(stderr, "Invalid --ui-mode '{}'. Expected: web, headless.\n", uiMode);
    return EXIT_FAILURE;
  }

  bool nostate = program["--new"] == true;
  bool nodebug = program["--no-debug"] == true;
  int rpcPort = program.get<int>("--rpc-port");

  auto seed = time(NULL);
  auto path = resolve_content_root(get_selfpath());
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

  auto &loader = entt::locator<Loader>::emplace();
  loader.init(app.log);

  auto &gm = entt::locator<GameManager>::emplace();
  gm.init(app.log);
  jobs.add(gm.startJob, true);

  auto &rpcServer = entt::locator<rpc::Server>::emplace(rpcPort);
  rpc::registerSessionHandlers(rpcServer);
  rpc::registerGameHandlers(rpcServer);
  rpc::registerFrameHandlers(rpcServer);
  rpc::registerStateHandlers(rpcServer);
  rpc::registerComponentHandlers(rpcServer);
  rpc::registerConnectionHandlers(rpcServer);
  rpc::registerCodeHandlers(rpcServer);
  rpc::registerItemsHandlers(rpcServer);
  rpc::registerStorageHandlers(rpcServer);
  rpc::registerPatchHandlers(rpcServer);
  rpc::initEventBridge(rpcServer);
  if (!rpcServer.start()) {
    app.log.error("RPC server startup failed. Try another port with --rpc-port <port>.");
    app.log.stop(APP_NAME);
    ix::uninitNetSystem();
    return EXIT_FAILURE;
  }

  while (true) {
    app.serve();
    gm.serve();
    std::this_thread::sleep_for(16ms);
  }
  rpcServer.stop();
  app.log.stop(APP_NAME);
  ix::uninitNetSystem();
  return EXIT_SUCCESS;
}
