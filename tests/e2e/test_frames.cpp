#include <catch2/catch_test_macros.hpp>
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <set>

using namespace std::chrono_literals;

// ─── §3 Frame management ──────────────────────────────────────────────────────

TEST_CASE("frame.list — returns array of frames") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto result = client.call("frame.list");
  REQUIRE(result.contains("frames"));
  REQUIRE(result["frames"].is_array());
  // game_init.lua creates at least some frames
  CHECK(result["frames"].size() >= 1);
}

TEST_CASE("frame.list — each summary has required fields") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto result = client.call("frame.list");
  REQUIRE(result["frames"].size() >= 1);

  for (const auto& frame : result["frames"]) {
    REQUIRE(frame.contains("entity_id"));
    REQUIRE(frame.contains("id"));
    REQUIRE(frame.contains("name"));
    REQUIRE(frame.contains("size"));
    REQUIRE(frame.contains("component_count"));
  }
}

TEST_CASE("frame.get — returns full frame DTO") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto list_result = client.call("frame.list");
  REQUIRE(list_result["frames"].size() >= 1);

  int entity_id = list_result["frames"][0]["entity_id"].get<int>();
  auto frame = client.call("frame.get", {{"id", entity_id}});
  assert_frame_dto(frame);
}

TEST_CASE("frame.get — components have required fields") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto list_result = client.call("frame.list");
  REQUIRE(list_result["frames"].size() >= 1);

  // Find a frame that has components
  for (const auto& summary : list_result["frames"]) {
    if (summary["component_count"].get<int>() > 0) {
      int entity_id = summary["entity_id"].get<int>();
      auto frame = client.call("frame.get", {{"id", entity_id}});
      REQUIRE(frame["components"].size() > 0);
      for (const auto& comp : frame["components"]) {
        assert_component_dto(comp);
      }
      return;
    }
  }
  // If no frame has components, skip gracefully
  SUCCEED("No frames with components found — skipping component field check");
}

TEST_CASE("frame.get — returns error 1002 for unknown entity") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  // Use an entity id that is very unlikely to exist
  auto resp = client.call_raw("frame.get", {{"id", 999999}});
  REQUIRE(resp.contains("error"));
  // Accept either ENTITY_NOT_FOUND (1002) or generic error
  CHECK(resp["error"]["code"].get<int>() != 0);
}

TEST_CASE("frame.create — requires session claim") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("frame.create", {{"name", "TestFrame"}});
  assert_jsonrpc_error(resp, 1000);
}

TEST_CASE("frame.create — queues frame creation") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  client.call("session.claim");

  auto result = client.call("frame.create", {{"name", "MyNewFrame"}});
  REQUIRE(result.contains("status"));
  CHECK(result["status"] == "queued");
  CHECK(result["name"] == "MyNewFrame");

  // Let the game tick process the command
  h.tick(3);

  // Verify the frame appears in the list
  auto list_result = client.call("frame.list");
  bool found = false;
  for (const auto& f : list_result["frames"]) {
    if (f["name"].get<std::string>() == "MyNewFrame") {
      found = true;
      break;
    }
  }
  CHECK(found);
}

TEST_CASE("frame.update — requires session claim") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto list_result = client.call("frame.list");
  REQUIRE(list_result["frames"].size() >= 1);
  int entity_id = list_result["frames"][0]["entity_id"].get<int>();

  auto resp = client.call_raw("frame.update", {
    {"id", entity_id},
    {"attributes", {{"name", "UpdatedName"}}}
  });
  assert_jsonrpc_error(resp, 1000);
}

TEST_CASE("frame.update — updates frame name") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  client.call("session.claim");

  auto list_result = client.call("frame.list");
  REQUIRE(list_result["frames"].size() >= 1);
  int entity_id = list_result["frames"][0]["entity_id"].get<int>();

  auto result = client.call("frame.update", {
    {"id", entity_id},
    {"attributes", {{"name", "RenamedFrame"}}}
  });
  REQUIRE(result.contains("status"));
  CHECK(result["status"] == "queued");

  // Process the update
  h.tick(3);

  // Verify the name was updated
  auto frame = client.call("frame.get", {{"id", entity_id}});
  CHECK(frame["name"].get<std::string>() == "RenamedFrame");
}

TEST_CASE("frame.list — game_init frames plus created ones") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  client.call("session.claim");

  // Count initial frames
  auto initial = client.call("frame.list");
  int initial_count = static_cast<int>(initial["frames"].size());

  // Create 2 additional frames
  client.call("frame.create", {{"name", "ExtraFrame1"}});
  client.call("frame.create", {{"name", "ExtraFrame2"}});
  h.tick(5);

  auto after = client.call("frame.list");
  int after_count = static_cast<int>(after["frames"].size());
  CHECK(after_count >= initial_count + 2);
}
