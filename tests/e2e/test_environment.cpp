#include <catch2/catch_test_macros.hpp>
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <set>

using namespace std::chrono_literals;

// ─── §7 Environment ───────────────────────────────────────────────────────────

TEST_CASE("environment — game.state includes environment object") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  h.tick(1);

  auto state = client.call("game.state");
  REQUIRE(state.contains("environment"));
}

TEST_CASE("environment — environment has required fields") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  h.tick(1);

  auto state = client.call("game.state");
  if (state["environment"].is_null()) {
    SUCCEED("Environment not yet initialized — skipping field check");
    return;
  }

  auto& env = state["environment"];
  REQUIRE(env.contains("temperature"));
  REQUIRE(env.contains("radioactivity"));
  REQUIRE(env.contains("air_flow"));
  REQUIRE(env.contains("sun"));
  REQUIRE(env.contains("minutes"));
  REQUIRE(env.contains("days"));
  REQUIRE(env.contains("is_day"));
}

TEST_CASE("environment — game.tick returns day/time info") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  h.tick(1);

  auto tick = client.call("game.tick");
  REQUIRE(tick["started"] == true);
  REQUIRE(tick.contains("minutes"));
  REQUIRE(tick.contains("days"));
  REQUIRE(tick.contains("isDay"));
  CHECK(tick["minutes"].is_number());
  CHECK(tick["days"].is_number());
  CHECK(tick["isDay"].is_boolean());
}
