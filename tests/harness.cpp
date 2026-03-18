#include "harness.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <filesystem>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <atomic>

#define SOL_SAFE_NUMERICS 1
#include <sol/sol.hpp>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
#include <utils/data/loader.hpp>
#include <utils/selfpath.hpp>
#include <lua/logger.hpp>
#include <game/game_manager.hpp>
#include <rpc/server.hpp>
#include <rpc/handlers/session_handler.hpp>
#include <rpc/handlers/game_handler.hpp>
#include <rpc/handlers/frame_handler.hpp>
#include <rpc/handlers/component_handler.hpp>
#include <rpc/handlers/connection_handler.hpp>
#include <rpc/handlers/code_handler.hpp>
#include <rpc/handlers/state_handler.hpp>
#include <rpc/event_bridge.hpp>
#include <ixwebsocket/IXNetSystem.h>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// Each TestHarness gets a unique port (sequential, starting at 19800)
static std::atomic<int> s_next_port{19800};

struct TestHarness::Impl {
  LibLog::Logger log = LibLog::Logger(fmt::color::white, "HARNESS");
  LibLog::Logger luaLog = LibLog::Logger(fmt::color::aqua, "LUA");

  std::thread game_thread_;
  std::atomic<bool> running_{false};
  int actual_port = 0;

  GameManager* gm_ptr = nullptr;
  rpc::Server* rpc_ptr = nullptr;

  void setup(int port_hint) {
    ix::initNetSystem();

    auto path = get_selfpath();

    entt::monostate<"id"_hs>{} = 0;
    entt::monostate<"path"_hs>{} = path;
    entt::monostate<"debug"_hs>{} = false;

    entt::locator<event_emitter>::emplace();

    entt::locator<sol::state>::emplace();
    auto& lua = entt::locator<sol::state>::value();
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string,
                       sol::lib::table, sol::lib::math, sol::lib::os);
    injectLogger(lua, luaLog);

    auto cp = path / "scripts" / "config.lua";
    if (!fs::exists(cp)) {
      throw std::runtime_error("config.lua not found at: " + cp.string());
    }
    lua.script_file(cp.string());

    initEnttLua();

    entt::locator<Loader>::emplace();
    auto& loader = entt::locator<Loader>::value();
    loader.init(log);

    auto& gm = entt::locator<GameManager>::emplace();
    gm_ptr = &gm;
    gm.headless = true;

    gm.init(log);
    gm.loadData();
    gm.start();

    auto& rpc = entt::locator<rpc::Server>::emplace(port_hint);
    rpc_ptr = &rpc;

    rpc::registerSessionHandlers(rpc);
    rpc::registerGameHandlers(rpc);
    rpc::registerFrameHandlers(rpc);
    rpc::registerComponentHandlers(rpc);
    rpc::registerConnectionHandlers(rpc);
    rpc::registerCodeHandlers(rpc);
    rpc::registerStateHandlers(rpc);
    rpc::initEventBridge(rpc);

    rpc.start();
    actual_port = rpc.port();

    std::this_thread::sleep_for(50ms);

    running_ = true;
    game_thread_ = std::thread([this]() {
      while (running_) {
        gm_ptr->serve();
        std::this_thread::sleep_for(16ms);
      }
    });
  }

  void teardown() {
    running_ = false;
    if (game_thread_.joinable()) {
      game_thread_.join();
    }
    if (rpc_ptr) {
      rpc_ptr->stop();
    }
    ix::uninitNetSystem();
  }

  void tick(int n) {
    if (!gm_ptr) return;
    uint64_t target = gm_ptr->tick_count() + static_cast<uint64_t>(n);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (gm_ptr->tick_count() < target) {
      if (std::chrono::steady_clock::now() > deadline)
        throw std::runtime_error("tick() timeout");
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
};

TestHarness::TestHarness(int port)
    : impl_(std::make_unique<Impl>())
    , port_(0) {
  int p = (port == 0) ? s_next_port.fetch_add(1) : port;
  impl_->setup(p);
  port_ = impl_->actual_port;
}

TestHarness::~TestHarness() {
  impl_->teardown();
}

void TestHarness::tick(int n) {
  impl_->tick(n);
}

GameManager& TestHarness::game_manager() {
  return *impl_->gm_ptr;
}

rpc::Server& TestHarness::rpc_server() {
  return *impl_->rpc_ptr;
}
