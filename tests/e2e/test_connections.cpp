#include "e2e_catch.hpp"
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <set>

using namespace std::chrono_literals;

// ─── §5 Connection management ─────────────────────────────────────────────────

E2E_TEST(connections,"game.state — returns connections array") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto state = client.call("game.state");
  REQUIRE(state.contains("connections"));
  REQUIRE(state["connections"].is_array());
}

E2E_TEST(connections,"game.state — connections have required DTO fields") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto state = client.call("game.state");
  for (const auto& conn : state["connections"]) {
    assert_connection_dto(conn);
  }
}

E2E_TEST(connections,"connection type is POWER or DATA") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto state = client.call("game.state");
  std::set<std::string> valid_types = {"POWER", "DATA"};
  for (const auto& conn : state["connections"]) {
    std::string type = conn["type"].get<std::string>();
    CHECK(valid_types.count(type) > 0);
  }
}

E2E_TEST(connections,"connection — source and target are integers") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto state = client.call("game.state");
  for (const auto& conn : state["connections"]) {
    REQUIRE(conn["source"].is_number_integer());
    REQUIRE(conn["target"].is_number_integer());
  }
}

E2E_TEST(connections,"connection — connection id is an integer") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto state = client.call("game.state");
  for (const auto& conn : state["connections"]) {
    REQUIRE(conn["id"].is_number_integer());
  }
}
