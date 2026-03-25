#include "e2e_catch.hpp"
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <set>

using namespace std::chrono_literals;

// ─── §2 Session management ────────────────────────────────────────────────────

E2E_TEST(session,"session.info — returns correct shape before claim") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto result = client.call("session.info");
  REQUIRE(result.contains("claimed_by"));
  REQUIRE(result.contains("clients"));
  REQUIRE(result.contains("uptime_ms"));
  CHECK(result["claimed_by"].is_null());
  CHECK(result["clients"].get<int>() >= 1);
  CHECK(result["uptime_ms"].get<int64_t>() >= 0);
}

E2E_TEST(session,"session.claim — succeeds for first caller") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto result = client.call("session.claim");
  CHECK(result["ok"] == true);
}

E2E_TEST(session,"session.claim — second caller gets error 1000") {
  TestHarness h;
  RpcClient client1, client2;
  client1.connect(h.ws_url());
  client2.connect(h.ws_url());

  client1.call("session.claim");

  auto resp = client2.call_raw("session.claim");
  assert_jsonrpc_error(resp, 1000);
}

E2E_TEST(session,"session.release — owner can release") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  client.call("session.claim");
  auto result = client.call("session.release");
  CHECK(result["status"] == "released");

  // Now another client can claim
  RpcClient client2;
  client2.connect(h.ws_url());
  auto result2 = client2.call("session.claim");
  CHECK(result2["ok"] == true);
}

E2E_TEST(session,"session.release — non-owner gets error 1000") {
  TestHarness h;
  RpcClient client1, client2;
  client1.connect(h.ws_url());
  client2.connect(h.ws_url());

  client1.call("session.claim");

  auto resp = client2.call_raw("session.release");
  assert_jsonrpc_error(resp, 1000);
}

E2E_TEST(session,"session.info — shows claimed_by after claim") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  client.call("session.claim");
  auto result = client.call("session.info");
  REQUIRE(result.contains("claimed_by"));
  CHECK(!result["claimed_by"].is_null());
}

E2E_TEST(session,"session — disconnect releases claim automatically") {
  TestHarness h;

  {
    RpcClient client;
    client.connect(h.ws_url());
    client.call("session.claim");
    // client goes out of scope here, disconnects
  }

  // Give server time to handle disconnect
  std::this_thread::sleep_for(200ms);

  // Another client should now be able to claim
  RpcClient client2;
  client2.connect(h.ws_url());
  auto result = client2.call("session.claim");
  CHECK(result["ok"] == true);
}
