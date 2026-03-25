#include "e2e_catch.hpp"
#include "harness.hpp"
#include "rpc_client.hpp"

E2E_TEST(code,"code loop: code.get_script returns current script") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto result = client.call("code.get_script", {{"frame_id", 1}});
  REQUIRE(result.contains("script"));
  REQUIRE(result["script"].is_string());
}

E2E_TEST(code,"code loop: code.execute returns status/error payload") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  auto result = client.call("code.execute", {{"frame_id", 1}, {"function", "update"}});
  REQUIRE(result.contains("status"));
  REQUIRE(result["status"].is_string());
  if (result["status"] != "ok") {
    REQUIRE(result.contains("error"));
    REQUIRE(result["error"].is_string());
  }
}
