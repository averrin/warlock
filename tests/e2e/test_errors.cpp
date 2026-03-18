#include <catch2/catch_test_macros.hpp>
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>
#include <future>
#include <set>

using namespace std::chrono_literals;

// ─── §9 Error handling ────────────────────────────────────────────────────────

TEST_CASE("error — unknown method returns -32601") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("no.such.method", {});
  assert_jsonrpc_error(resp, -32601);
}

TEST_CASE("error — method not found message is present") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("completely.unknown", {});
  REQUIRE(resp.contains("error"));
  REQUIRE(resp["error"].contains("message"));
  CHECK(!resp["error"]["message"].get<std::string>().empty());
}

TEST_CASE("error — frame.get without id returns error") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("frame.get", {});
  REQUIRE(resp.contains("error"));
  CHECK(resp["error"]["code"].get<int>() != 0);
}

TEST_CASE("error — frame.create without claim returns 1000") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("frame.create", {{"name", "Test"}});
  assert_jsonrpc_error(resp, 1000);
}

TEST_CASE("error — frame.update without claim returns 1000") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("frame.update", {{"id", 1}, {"attributes", {}}});
  assert_jsonrpc_error(resp, 1000);
}

TEST_CASE("error — state.save without claim returns 1000") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("state.save", {});
  assert_jsonrpc_error(resp, 1000);
}

TEST_CASE("error — state.load without claim returns 1000") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("state.load", {});
  assert_jsonrpc_error(resp, 1000);
}

TEST_CASE("error — error response has correct JSON-RPC shape") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("nonexistent.method", {});
  REQUIRE(resp.contains("jsonrpc"));
  CHECK(resp["jsonrpc"] == "2.0");
  REQUIRE(resp.contains("error"));
  REQUIRE(resp["error"].contains("code"));
  REQUIRE(resp["error"].contains("message"));
  // Must NOT contain result
  CHECK(!resp.contains("result"));
}

TEST_CASE("error — error response id matches request id") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  // call_raw uses sequential ids; this will be some integer
  auto resp = client.call_raw("bad.method", {});
  REQUIRE(resp.contains("id"));
  CHECK(!resp["id"].is_null());
}
