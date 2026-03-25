#include "e2e_catch.hpp"
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <set>

using namespace std::chrono_literals;

// ─── §17 Regression tests ─────────────────────────────────────────────────────

E2E_TEST(regression,"regression — session can be claimed after previous owner disconnects") {
  // Tests that disconnecting a client properly releases the session claim
  TestHarness h;

  {
    RpcClient owner;
    owner.connect(h.ws_url());
    owner.call("session.claim");
    auto info = owner.call("session.info");
    CHECK(!info["claimed_by"].is_null());
    // owner disconnects here
  }

  std::this_thread::sleep_for(200ms);

  RpcClient new_owner;
  new_owner.connect(h.ws_url());
  auto result = new_owner.call("session.claim");
  CHECK(result["ok"] == true);
}

E2E_TEST(regression,"regression — frame.get returns position field") {
  // Ensures that frame DTOs always include position even when transform is missing
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto list = client.call("frame.list");
  REQUIRE(list["frames"].size() >= 1);

  int entity_id = list["frames"][0]["entity_id"].get<int>();
  auto frame = client.call("frame.get", {{"id", entity_id}});

  REQUIRE(frame.contains("position"));
  REQUIRE(frame["position"].contains("x"));
  REQUIRE(frame["position"].contains("y"));
}

E2E_TEST(regression,"regression — multiple concurrent session.info calls do not crash") {
  // Regression for race conditions in session info
  TestHarness h;

  constexpr int N = 10;
  std::vector<RpcClient> clients(N);
  for (auto& c : clients) {
    c.connect(h.ws_url());
  }

  std::vector<std::future<json>> futures;
  for (auto& c : clients) {
    futures.push_back(std::async(std::launch::async, [&c]() {
      return c.call("session.info");
    }));
  }

  for (auto& f : futures) {
    auto result = f.get();
    REQUIRE(result.contains("clients"));
    REQUIRE(result.contains("uptime_ms"));
  }
}

E2E_TEST(regression,"regression — frame.create then frame.get returns correct data") {
  // Ensures created frame can be retrieved immediately after tick processing
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  client.call("session.claim");
  client.call("frame.create", {{"name", "RegressionTestFrame"}});
  h.tick(5);

  auto list = client.call("frame.list");
  int entity_id = -1;
  for (const auto& f : list["frames"]) {
    if (f["name"].get<std::string>() == "RegressionTestFrame") {
      entity_id = f["entity_id"].get<int>();
      break;
    }
  }
  REQUIRE(entity_id != -1);

  auto frame = client.call("frame.get", {{"id", entity_id}});
  CHECK(frame["name"].get<std::string>() == "RegressionTestFrame");
  REQUIRE(frame.contains("components"));
  REQUIRE(frame.contains("position"));
}

E2E_TEST(regression,"regression — game.state frame count matches frame.list count") {
  // Ensures game.state and frame.list return the same number of frames
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto state = client.call("game.state");
  auto list = client.call("frame.list");

  CHECK(state["frames"].size() == list["frames"].size());
}
