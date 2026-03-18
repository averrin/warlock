#include <catch2/catch_test_macros.hpp>
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <set>

using namespace std::chrono_literals;

// ─── §6 Code execution ────────────────────────────────────────────────────────

TEST_CASE("game.state — started is true after init") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto state = client.call("game.state");
  REQUIRE(state.contains("started"));
  CHECK(state["started"] == true);
}

TEST_CASE("game.tick — returns timing info when started") {
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

TEST_CASE("game.state — returns frames and connections") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto state = client.call("game.state");
  REQUIRE(state.contains("frames"));
  REQUIRE(state["frames"].is_array());
  REQUIRE(state.contains("connections"));
  REQUIRE(state["connections"].is_array());
}

TEST_CASE("game.state — includes environment when game started") {
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
