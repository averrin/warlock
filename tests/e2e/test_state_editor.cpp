#include "e2e_catch.hpp"
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

// ─── Autosave configure ─────────────────────────────────────────────────────

E2E_TEST(state_editor,"state.autosave.status — returns defaults") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto result = client.call("state.autosave.status");
  CHECK(result["enabled"] == false);
  CHECK(result["interval_seconds"] == 60);
  CHECK(result.contains("last_saved_at"));
}

E2E_TEST(state_editor,"state.autosave.configure — requires claim") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("state.autosave.configure", {{"enabled", true}});
  assert_jsonrpc_error(resp, 1000);
}

E2E_TEST(state_editor,"state.autosave.configure — enables autosave") {
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

E2E_TEST(state_editor,"state.autosave.configure — clamps interval minimum") {
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

E2E_TEST(state_editor,"env.set_field — requires claim") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");
  client.call("game.start");
  std::this_thread::sleep_for(200ms);
  h.tick(5);

  client.call("session.release");
  auto resp = client.call_raw("env.set_field", {{"field", "temperature"}, {"value", 42.0}});
  assert_jsonrpc_error(resp, 1000);
}

E2E_TEST(state_editor,"env.set_field — sets temperature") {
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

E2E_TEST(state_editor,"env.set_field — rejects unknown field") {
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

E2E_TEST(state_editor,"state.save — broadcasts notify.state.saved") {
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

  auto notifications = client.drain_events();
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

// ─── entities.* (registry inspector) ─────────────────────────────────────────

E2E_TEST(state_editor, "entities.create / entities.list — hierarchy uses relation") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");
  client.call("game.start");
  std::this_thread::sleep_for(200ms);
  h.tick(5);

  auto list0 = client.call("entities.list");
  REQUIRE(list0.contains("entities"));
  int frames_folder = -1;
  for (const auto& ent : list0["entities"]) {
    if (!ent.contains("components")) continue;
    const auto& comps = ent["components"];
    if (comps.contains("meta") && comps["meta"].contains("name") &&
        comps["meta"]["name"].get<std::string>() == "Frames") {
      frames_folder = ent["entity_id"].get<int>();
      break;
    }
  }
  REQUIRE(frames_folder >= 0);

  auto created = client.call("entities.create", {{"name", "E2E Empty"}, {"parent_entity_id", frames_folder}});
  int new_id = created["entity_id"].get<int>();

  auto list1 = client.call("entities.list");
  bool found_child = false;
  for (const auto& ent : list1["entities"]) {
    if (ent["entity_id"].get<int>() != new_id) continue;
    REQUIRE(ent["components"].contains("relation"));
    int p = ent["components"]["relation"]["parent"].get<int>();
    CHECK(p == frames_folder);
    found_child = true;
    break;
  }
  CHECK(found_child);

  client.call("entities.destroy", {{"entity_id", new_id}});
}
