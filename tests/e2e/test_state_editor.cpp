#include <catch2/catch_test_macros.hpp>
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

// ─── Autosave configure ─────────────────────────────────────────────────────

TEST_CASE("state.autosave.status — returns defaults") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto result = client.call("state.autosave.status");
  CHECK(result["enabled"] == false);
  CHECK(result["interval_seconds"] == 60);
  CHECK(result["last_saved_at"].is_null());
}

TEST_CASE("state.autosave.configure — requires claim") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("state.autosave.configure", {{"enabled", true}});
  assert_jsonrpc_error(resp, 1001);
}

TEST_CASE("state.autosave.configure — enables autosave") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");

  auto result = client.call("state.autosave.configure", {
    {"enabled", true},
    {"interval_seconds", 30}
  });
  CHECK(result["enabled"] == true);
  CHECK(result["interval_seconds"] == 30);

  // Verify via status
  auto status = client.call("state.autosave.status");
  CHECK(status["enabled"] == true);
  CHECK(status["interval_seconds"] == 30);
}

TEST_CASE("state.autosave.configure — clamps interval minimum") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");

  auto result = client.call("state.autosave.configure", {
    {"enabled", true},
    {"interval_seconds", 1}
  });
  CHECK(result["interval_seconds"].get<int>() >= 5);
}

// ─── env.set_field ──────────────────────────────────────────────────────────

TEST_CASE("env.set_field — requires claim") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");
  client.call("game.start");
  std::this_thread::sleep_for(200ms);
  h.tick(5);

  client.call("session.release");
  auto resp = client.call_raw("env.set_field", {{"field", "temperature"}, {"value", 42.0}});
  assert_jsonrpc_error(resp, 1001);
}

TEST_CASE("env.set_field — sets temperature") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");
  client.call("game.start");
  std::this_thread::sleep_for(200ms);
  h.tick(5);

  auto result = client.call("env.set_field", {{"field", "temperature"}, {"value", 42.5}});
  CHECK(result["ok"] == true);

  auto env = client.call("env.status");
  CHECK(env["env"]["temperature"].get<float>() > 40.0f);
}

TEST_CASE("env.set_field — rejects unknown field") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");
  client.call("game.start");
  std::this_thread::sleep_for(200ms);
  h.tick(5);

  auto resp = client.call_raw("env.set_field", {{"field", "bogus"}, {"value", 1.0}});
  assert_jsonrpc_error(resp, -32602);
}

// ─── state.save broadcast ───────────────────────────────────────────────────

TEST_CASE("state.save — broadcasts notify.state.saved") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");
  client.call("game.start");
  std::this_thread::sleep_for(200ms);
  h.tick(5);

  client.call("state.subscribe", {{"topics", {"notify.state.saved"}}});

  auto result = client.call("state.save");
  CHECK(result["status"] == "queued");

  // Tick to process the queued save
  h.tick(2);
  std::this_thread::sleep_for(200ms);

  auto notifications = client.drain_notifications();
  bool found = false;
  for (auto& n : notifications) {
    if (n.contains("method") && n["method"] == "notify.state.saved") {
      found = true;
      CHECK(n["params"]["auto"] == false);
      CHECK(n["params"]["saved_at"].is_string());
    }
  }
  CHECK(found);
}
