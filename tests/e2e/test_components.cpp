#include <catch2/catch_test_macros.hpp>
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <set>

using namespace std::chrono_literals;

// ─── §4 Component management ──────────────────────────────────────────────────

// Helper: find first frame that has at least one component
static int find_frame_with_components(RpcClient& client) {
  auto list = client.call("frame.list");
  for (const auto& summary : list["frames"]) {
    if (summary["component_count"].get<int>() > 0) {
      return summary["entity_id"].get<int>();
    }
  }
  return -1;
}

TEST_CASE("component — frame.get returns components array") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto list = client.call("frame.list");
  REQUIRE(list["frames"].size() >= 1);

  int entity_id = list["frames"][0]["entity_id"].get<int>();
  auto frame = client.call("frame.get", {{"id", entity_id}});

  REQUIRE(frame.contains("components"));
  REQUIRE(frame["components"].is_array());
}

TEST_CASE("component — each component has required DTO fields") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  int entity_id = find_frame_with_components(client);
  if (entity_id == -1) {
    SUCCEED("No frames with components in initial state — skipping");
    return;
  }

  auto frame = client.call("frame.get", {{"id", entity_id}});
  for (const auto& comp : frame["components"]) {
    assert_component_dto(comp);
  }
}

TEST_CASE("component — state is a valid enum value") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  int entity_id = find_frame_with_components(client);
  if (entity_id == -1) {
    SUCCEED("No frames with components in initial state — skipping");
    return;
  }

  std::set<std::string> valid_states = {
    "DEACTIVATED", "ACTIVATING", "ACTIVE", "DEACTIVATING",
    "COMP_ERROR", "DESTROYED", "BLOCKED", "BROKEN"
  };

  auto frame = client.call("frame.get", {{"id", entity_id}});
  for (const auto& comp : frame["components"]) {
    std::string state = comp["state"].get<std::string>();
    CHECK(valid_states.count(state) > 0);
  }
}

TEST_CASE("component — attributes is an object") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  int entity_id = find_frame_with_components(client);
  if (entity_id == -1) {
    SUCCEED("No frames with components in initial state — skipping");
    return;
  }

  auto frame = client.call("frame.get", {{"id", entity_id}});
  for (const auto& comp : frame["components"]) {
    REQUIRE(comp["attributes"].is_object());
  }
}

TEST_CASE("component — size is valid enum value") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  int entity_id = find_frame_with_components(client);
  if (entity_id == -1) {
    SUCCEED("No frames with components in initial state — skipping");
    return;
  }

  std::set<std::string> valid_sizes = {"S", "M", "L"};
  auto frame = client.call("frame.get", {{"id", entity_id}});
  for (const auto& comp : frame["components"]) {
    std::string size = comp["size"].get<std::string>();
    CHECK(valid_sizes.count(size) > 0);
  }
}
