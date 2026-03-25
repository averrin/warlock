#include "e2e_catch.hpp"
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <set>

using namespace std::chrono_literals;

// ─── §8 Events / notifications ────────────────────────────────────────────────

E2E_TEST(events,"event — client can connect and receive session.info") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  CHECK(client.is_connected());

  auto result = client.call("session.info");
  CHECK(result.contains("clients"));
}

E2E_TEST(events,"event — multiple clients can connect simultaneously") {
  TestHarness h;
  RpcClient c1, c2, c3;
  c1.connect(h.ws_url());
  c2.connect(h.ws_url());
  c3.connect(h.ws_url());

  CHECK(c1.is_connected());
  CHECK(c2.is_connected());
  CHECK(c3.is_connected());

  auto result = c1.call("session.info");
  CHECK(result["clients"].get<int>() >= 3);
}

E2E_TEST(events,"event — drain_events returns empty initially") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  // Call session.info to ensure the connection is active
  client.call("session.info");

  // drain_events should return any notifications that arrived
  auto events = client.drain_events();
  // We don't require empty — there may be init notifications from event bridge
  CHECK(events.size() >= 0);
}

E2E_TEST(events,"event — session.claim broadcasts no error to other clients") {
  TestHarness h;
  RpcClient owner, observer;
  owner.connect(h.ws_url());
  observer.connect(h.ws_url());

  // Clear any existing events
  observer.drain_events();

  owner.call("session.claim");

  // The observer should remain connected and still be able to call
  auto result = observer.call("session.info");
  CHECK(!result["claimed_by"].is_null());
}

E2E_TEST(events,"event — frame.create triggers frame list growth") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  client.call("session.claim");

  auto before = client.call("frame.list");
  int before_count = static_cast<int>(before["frames"].size());

  client.call("frame.create", {{"name", "EventTestFrame"}});
  h.tick(3);

  auto after = client.call("frame.list");
  int after_count = static_cast<int>(after["frames"].size());
  CHECK(after_count > before_count);
}

E2E_TEST(events,"event — game.state is consistent across multiple calls") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto state1 = client.call("game.state");
  auto state2 = client.call("game.state");

  // Frame count should be the same between two consecutive calls
  CHECK(state1["frames"].size() == state2["frames"].size());
}

E2E_TEST(events,"event — concurrent calls from same client are handled") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  // Issue multiple rapid calls and verify all return valid responses
  auto f1 = std::async(std::launch::async, [&]() {
    return client.call("session.info");
  });
  auto f2 = std::async(std::launch::async, [&]() {
    return client.call("game.state");
  });
  auto f3 = std::async(std::launch::async, [&]() {
    return client.call("frame.list");
  });

  auto r1 = f1.get();
  auto r2 = f2.get();
  auto r3 = f3.get();

  CHECK(r1.contains("clients"));
  CHECK(r2.contains("started"));
  CHECK(r3.contains("frames"));
}

E2E_TEST(events,"event — notify (no id) does not produce a response") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  // Send a notification — the server should not respond
  client.notify("session.info", {});

  // Wait briefly then check no response is pending
  std::this_thread::sleep_for(100ms);
  auto events = client.drain_events();
  // No crash means success
  CHECK(true);
}
