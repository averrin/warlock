#include <catch2/catch_test_macros.hpp>
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <set>
#include <vector>
#include <atomic>

using namespace std::chrono_literals;

// ─── §10 Concurrency ─────────────────────────────────────────────────────────

TEST_CASE("concurrency — multiple clients read simultaneously") {
  TestHarness h;

  constexpr int N_CLIENTS = 5;
  std::vector<RpcClient> clients(N_CLIENTS);
  for (auto& c : clients) {
    c.connect(h.ws_url());
  }

  // All clients issue frame.list simultaneously
  std::vector<std::future<json>> futures;
  for (auto& c : clients) {
    futures.push_back(std::async(std::launch::async, [&c]() {
      return c.call("frame.list");
    }));
  }

  // All should succeed
  for (auto& f : futures) {
    auto result = f.get();
    REQUIRE(result.contains("frames"));
    CHECK(result["frames"].is_array());
  }
}

TEST_CASE("concurrency — rapid sequential calls are handled correctly") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  client.call("session.claim");

  // Issue 10 rapid frame.create calls
  std::vector<std::future<json>> futures;
  for (int i = 0; i < 10; i++) {
    std::string name = "ConcurrentFrame" + std::to_string(i);
    futures.push_back(std::async(std::launch::async, [&client, name]() {
      return client.call("frame.create", {{"name", name}});
    }));
  }

  // All should return "queued" status
  for (auto& f : futures) {
    auto result = f.get();
    REQUIRE(result.contains("status"));
    CHECK(result["status"] == "queued");
  }

  // Give the game loop time to process all commands
  h.tick(20);

  // At least some frames should have been created
  auto list = client.call("frame.list");
  CHECK(list["frames"].size() >= 10);
}

TEST_CASE("concurrency — game.state is consistent under concurrent reads") {
  TestHarness h;

  constexpr int N_ITERATIONS = 20;
  RpcClient reader1, reader2;
  reader1.connect(h.ws_url());
  reader2.connect(h.ws_url());

  std::atomic<int> success_count{0};

  // Both clients read game.state concurrently many times
  auto read_task = [&](RpcClient& c) {
    for (int i = 0; i < N_ITERATIONS; i++) {
      try {
        auto state = c.call("game.state");
        if (state.contains("started") && state.contains("frames")) {
          success_count++;
        }
      } catch (...) {
        // Count failures implicitly via success_count not reaching max
      }
    }
  };

  auto f1 = std::async(std::launch::async, [&]() { read_task(reader1); });
  auto f2 = std::async(std::launch::async, [&]() { read_task(reader2); });

  f1.get();
  f2.get();

  // All reads should have succeeded
  CHECK(success_count.load() == N_ITERATIONS * 2);
}
