#include "e2e_catch.hpp"
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <set>

using namespace std::chrono_literals;

// ─── §6 Code execution ────────────────────────────────────────────────────────

E2E_TEST(code,"game.state — started is true after init") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto state = client.call("game.state");
  REQUIRE(state.contains("started"));
  CHECK(state["started"] == true);
}

E2E_TEST(code,"game.tick — returns timing info when started") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  h.tick(1);

  auto tick = client.call("game.tick");
  REQUIRE(tick.contains("started"));
  CHECK(tick["started"] == true);
  REQUIRE(tick.contains("minutes"));
  REQUIRE(tick.contains("days"));
  REQUIRE(tick.contains("isDay"));
}

E2E_TEST(code,"game.state — returns frames and connections") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto state = client.call("game.state");
  REQUIRE(state.contains("frames"));
  REQUIRE(state["frames"].is_array());
  REQUIRE(state.contains("connections"));
  REQUIRE(state["connections"].is_array());
}

E2E_TEST(code,"game.state — includes environment when game started") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  h.tick(1);

  auto state = client.call("game.state");
  REQUIRE(state.contains("environment"));
  if (!state["environment"].is_null()) {
    auto& env = state["environment"];
    REQUIRE(env.contains("temperature"));
    REQUIRE(env.contains("minutes"));
    REQUIRE(env.contains("days"));
    REQUIRE(env.contains("is_day"));
  }
}
