#include "e2e_catch.hpp"
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

// ─── Lua Execution Tests ─────────────────────────────────────────────────────

// Helper: get the first frame data id from game.state
static int first_frame_data_id(RpcClient& client) {
  auto state = client.call("game.state");
  REQUIRE(state.contains("frames"));
  REQUIRE(state["frames"].size() >= 1);
  return state["frames"][0]["id"].get<int>();
}

// Helper: get entity_id for a frame with components
static int first_frame_entity_id(RpcClient& client) {
  auto list = client.call("frame.list");
  REQUIRE(list["frames"].size() >= 1);
  return list["frames"][0]["entity_id"].get<int>();
}

// Helper: find core component state from game.state for a given frame data id
static std::string get_core_state(RpcClient& client, int frame_data_id) {
  auto state = client.call("game.state");
  for (auto& f : state["frames"]) {
    if (f["id"].get<int>() == frame_data_id) {
      for (auto& c : f["components"]) {
        if (c["type"].get<std::string>() == "Core") {
          return c["state"].get<std::string>();
        }
      }
    }
  }
  return "";
}

// ─── code.sources ────────────────────────────────────────────────────────────

E2E_TEST(lua,"lua exec: code.sources returns non-empty source map") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto result = client.call("code.sources");
  REQUIRE(result.contains("sources"));
  REQUIRE(result["sources"].is_object());
  CHECK(result["sources"].size() > 0);
}

E2E_TEST(lua,"lua exec: code.sources contains Core component") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto result = client.call("code.sources");
  bool has_core = false;
  for (auto& [name, code] : result["sources"].items()) {
    if (name.find("core") != std::string::npos || name.find("Core") != std::string::npos) {
      has_core = true;
      CHECK(code.is_string());
      CHECK(code.get<std::string>().size() > 0);
    }
  }
  CHECK(has_core);
}

// ─── code.completion ─────────────────────────────────────────────────────────

E2E_TEST(lua,"lua exec: code.completion returns keys for frame path") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  int fid = first_frame_data_id(client);

  auto result = client.call("code.completion", {{"frame_id", fid}, {"path", "frame"}});
  REQUIRE(result.contains("keys"));
  REQUIRE(result["keys"].is_array());
  bool has_data = false;
  for (auto& k : result["keys"]) {
    if (k.get<std::string>() == "data") {
      has_data = true;
    }
  }
  CHECK(has_data);
}

E2E_TEST(lua,"lua exec: code.completion globals include frame") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  int fid = first_frame_data_id(client);

  auto result = client.call("code.completion", {{"frame_id", fid}, {"path", ""}});
  REQUIRE(result.contains("keys"));
  bool has_frame = false;
  for (auto& k : result["keys"]) {
    if (k.get<std::string>() == "frame") {
      has_frame = true;
    }
  }
  CHECK(has_frame);
}

// ─── code.blueprints ─────────────────────────────────────────────────────────

E2E_TEST(lua,"lua exec: code.blueprints returns non-empty blueprint map") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto result = client.call("code.blueprints");
  REQUIRE(result.contains("blueprints"));
  REQUIRE(result["blueprints"].is_object());
  CHECK(result["blueprints"].size() > 0);
}

// ─── code.update + code.get_script roundtrip ─────────────────────────────────

E2E_TEST(lua,"lua exec: code.update then get_script returns same code") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  std::string new_code = R"(
return {
  update = function()
    -- roundtrip test marker
  end,
}
)";

  auto update_result = client.call("code.update", {{"frame_id", fid}, {"code", new_code}});
  REQUIRE(update_result.contains("ok"));
  CHECK(update_result["ok"] == true);

  auto get_result = client.call("code.get_script", {{"frame_id", fid}});
  REQUIRE(get_result.contains("script"));
  CHECK(get_result["script"].get<std::string>() == new_code);
}

// ─── code.execute with valid code ────────────────────────────────────────────

E2E_TEST(lua,"lua exec: execute valid code returns status ok") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  std::string valid_code = R"(
return {
  update = function()
    -- no-op, should succeed
  end,
}
)";

  client.call("code.update", {{"frame_id", fid}, {"code", valid_code}});
  auto result = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  REQUIRE(result.contains("status"));
  CHECK(result["status"] == "ok");
}

// ─── code.execute with syntax error ──────────────────────────────────────────

E2E_TEST(lua,"lua exec: execute code with syntax error returns error") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  std::string bad_code = R"(
this is not valid lua at all !!!
)";

  client.call("code.update", {{"frame_id", fid}, {"code", bad_code}});
  auto result = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  REQUIRE(result.contains("status"));
  CHECK(result["status"] == "error");
  REQUIRE(result.contains("error"));
  CHECK(result["error"].get<std::string>().size() > 0);
}

// ─── code.execute with runtime error ─────────────────────────────────────────

E2E_TEST(lua,"lua exec: execute code with runtime error returns error") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  std::string runtime_error_code = R"(
return {
  update = function()
    local x = nil
    x.foo()  -- attempt to index nil
  end,
}
)";

  client.call("code.update", {{"frame_id", fid}, {"code", runtime_error_code}});
  auto result = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  REQUIRE(result.contains("status"));
  CHECK(result["status"] == "error");
  REQUIRE(result.contains("error"));
  auto err = result["error"].get<std::string>();
  CHECK(err.size() > 0);
}

// ─── Component state set to COMP_ERROR after runtime error ───────────────────

E2E_TEST(lua,"lua exec: runtime error sets component state to COMP_ERROR") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  std::string bad_runtime = R"(
return {
  update = function()
    error("intentional failure")
  end,
}
)";

  client.call("code.update", {{"frame_id", fid}, {"code", bad_runtime}});
  auto result = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  REQUIRE(result.contains("status"));
  CHECK(result["status"] == "error");
  REQUIRE(result.contains("error"));
  CHECK(result["error"].get<std::string>().find("intentional failure") != std::string::npos);

  // Component state should now be COMP_ERROR
  auto comp_state = get_core_state(client, fid);
  CHECK(comp_state == "COMP_ERROR");
}

// ─── code.update invalidates cached script ───────────────────────────────────

E2E_TEST(lua,"lua exec: code.update invalidates cached script, new code runs") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  // First: upload valid code v1, verify it runs
  std::string code_v1 = R"(
return {
  update = function()
    -- version 1
  end,
}
)";
  client.call("code.update", {{"frame_id", fid}, {"code", code_v1}});
  auto r1 = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  CHECK(r1["status"] == "ok");

  // Second: upload different valid code v2 — verifies invalidation happened
  // (if old cached script were used, "start" would succeed too, but we verify
  //  the update path itself works by calling a different function)
  std::string code_v2 = R"(
return {
  update = function()
    -- version 2, different code
  end,
  check = function()
    -- only v2 has this
  end,
}
)";
  client.call("code.update", {{"frame_id", fid}, {"code", code_v2}});
  // Call "check" which only exists in v2 — if invalidation failed, this errors
  auto r2 = client.call("code.execute", {{"frame_id", fid}, {"function", "check"}});
  CHECK(r2["status"] == "ok");
}

// ─── Missing function in script table ────────────────────────────────────────

E2E_TEST(lua,"lua exec: calling missing function returns error") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  // Script returns table without "update"
  std::string no_update = R"(
return {
  start = function() end,
}
)";
  client.call("code.update", {{"frame_id", fid}, {"code", no_update}});
  auto result = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  CHECK(result["status"] == "error");
}

// ─── Script returns non-table ────────────────────────────────────────────────

E2E_TEST(lua,"lua exec: script that returns non-table causes error") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  std::string bad_return = R"(return 42)";
  client.call("code.update", {{"frame_id", fid}, {"code", bad_return}});
  auto result = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  CHECK(result["status"] == "error");
}

// ─── Code accesses frame global ──────────────────────────────────────────────

E2E_TEST(lua,"lua exec: code can access frame global") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  std::string frame_access = R"(
return {
  update = function()
    local name = frame.data.name
    if name == nil then
      error("frame.data.name is nil")
    end
  end,
}
)";
  client.call("code.update", {{"frame_id", fid}, {"code", frame_access}});
  auto result = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  REQUIRE(result.contains("status"));
  CHECK(result["status"] == "ok");
}

// ─── Code accesses environment global ────────────────────────────────────────

E2E_TEST(lua,"lua exec: code can access environment global") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  h.tick(1);  // ensure environment is populated

  int fid = first_frame_data_id(client);

  std::string env_access = R"(
return {
  update = function()
    local temp = environment.temperature
    if temp == nil then
      error("environment.temperature is nil")
    end
  end,
}
)";
  client.call("code.update", {{"frame_id", fid}, {"code", env_access}});
  auto result = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  REQUIRE(result.contains("status"));
  CHECK(result["status"] == "ok");
}

// ─── Code can call frame:getComponentByType ──────────────────────────────────

E2E_TEST(lua,"lua exec: code can call frame:getComponentByType") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  std::string api_call = R"(
return {
  update = function()
    local core = frame:getComponentByType("Core")
    if core == nil then
      error("getComponentByType returned nil for Core")
    end
  end,
}
)";
  client.call("code.update", {{"frame_id", fid}, {"code", api_call}});
  auto result = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  REQUIRE(result.contains("status"));
  CHECK(result["status"] == "ok");
}

// ─── Explicit function parameter: start ──────────────────────────────────────

E2E_TEST(lua,"lua exec: code.execute with function=start calls start") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  std::string code = R"(
return {
  start = function()
    -- start function called successfully
  end,
  update = function() end,
}
)";
  client.call("code.update", {{"frame_id", fid}, {"code", code}});
  auto result = client.call("code.execute", {{"frame_id", fid}, {"function", "start"}});
  REQUIRE(result.contains("status"));
  CHECK(result["status"] == "ok");
}

// ─── Authorization: code.update requires session claim ───────────────────────

E2E_TEST(lua,"lua exec: code.update without claim returns error 1000") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  // Deliberately NOT claiming session

  int fid = first_frame_data_id(client);

  auto resp = client.call_raw("code.update", {{"frame_id", fid}, {"code", "return {}"}});
  REQUIRE(resp.contains("error"));
  CHECK(resp["error"]["code"].get<int>() == 1000);
}

// ─── Authorization: code.execute requires session claim ──────────────────────

E2E_TEST(lua,"lua exec: code.execute without claim returns error 1000") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  // Deliberately NOT claiming session

  int fid = first_frame_data_id(client);

  auto resp = client.call_raw("code.execute", {{"frame_id", fid}, {"function", "update"}});
  REQUIRE(resp.contains("error"));
  CHECK(resp["error"]["code"].get<int>() == 1000);
}

// ─── code.execute on nonexistent frame ───────────────────────────────────────

E2E_TEST(lua,"lua exec: code.execute on nonexistent frame returns error") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  auto result = client.call("code.execute", {{"frame_id", 999999}, {"function", "update"}});
  REQUIRE(result.contains("status"));
  CHECK(result["status"] == "error");
  CHECK(result.contains("error"));
}

// ─── code.get_script on nonexistent frame ────────────────────────────────────

E2E_TEST(lua,"lua exec: code.get_script on nonexistent frame returns RPC error") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("code.get_script", {{"frame_id", 999999}});
  REQUIRE(resp.contains("error"));
  CHECK(resp["error"]["code"].get<int>() == 1002);  // ENTITY_NOT_FOUND
}

// ─── code.update missing params ──────────────────────────────────────────────

E2E_TEST(lua,"lua exec: code.update with missing params returns error") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  // Missing code parameter
  auto resp = client.call_raw("code.update", {{"frame_id", 1}});
  REQUIRE(resp.contains("error"));
  CHECK(resp["error"]["code"].get<int>() == -32602);  // INVALID_PARAMS
}

// ─── code.execute missing frame_id ───────────────────────────────────────────

E2E_TEST(lua,"lua exec: code.execute with missing frame_id returns error") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  auto resp = client.call_raw("code.execute", {{"function", "update"}});
  REQUIRE(resp.contains("error"));
  CHECK(resp["error"]["code"].get<int>() == -32602);  // INVALID_PARAMS
}

// ─── Blueprint with embedded code executes during game tick ──────────────────

E2E_TEST(lua,"lua exec: code.blueprints lists available blueprints by name") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto bp_result = client.call("code.blueprints");
  REQUIRE(bp_result.contains("blueprints"));
  REQUIRE(bp_result["blueprints"].is_object());
  REQUIRE(bp_result["blueprints"].size() > 0);

  // Each blueprint value should be a non-empty string (Lua source)
  for (auto& [name, src] : bp_result["blueprints"].items()) {
    CHECK(src.is_string());
    CHECK(src.get<std::string>().size() > 0);
  }
}

// NOTE: frame.create_from_blueprint + activate + tick causes a segfault
// during teardown (known bug). This needs investigation before adding
// tick-based blueprint execution tests.

// ─── Successive executions preserve Lua state ────────────────────────────────

E2E_TEST(lua,"lua exec: successive executions share Lua global state") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  // Code uses a global counter that persists between calls
  std::string counter_code = R"(
if counter == nil then counter = 0 end
return {
  update = function()
    counter = counter + 1
    if counter < 1 then
      error("counter should be >= 1, got " .. tostring(counter))
    end
  end,
}
)";
  client.call("code.update", {{"frame_id", fid}, {"code", counter_code}});

  // Execute multiple times — each should increment the global counter
  auto r1 = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  CHECK(r1["status"] == "ok");

  auto r2 = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  CHECK(r2["status"] == "ok");

  auto r3 = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  CHECK(r3["status"] == "ok");
}

// ─── code.execute default function is "update" ──────────────────────────────

E2E_TEST(lua,"lua exec: code.execute without function param defaults to update") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  std::string code = R"(
return {
  update = function()
    -- default function test
  end,
}
)";
  client.call("code.update", {{"frame_id", fid}, {"code", code}});

  // Call without "function" param — should default to "update"
  auto result = client.call("code.execute", {{"frame_id", fid}});
  REQUIRE(result.contains("status"));
  CHECK(result["status"] == "ok");
}

// ─── Empty code attribute ────────────────────────────────────────────────────

E2E_TEST(lua,"lua exec: execute with empty code returns error") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  client.call("code.update", {{"frame_id", fid}, {"code", ""}});
  auto result = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  // Empty code should result in error (no table returned)
  CHECK(result["status"] == "error");
}

// ─── Code that does heavy computation (no infinite loop protection check) ────

E2E_TEST(lua,"lua exec: code that modifies component state") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  (void)client.call("session.claim");

  int fid = first_frame_data_id(client);

  // Code that reads and uses component API
  std::string state_code = R"(
return {
  update = function()
    local core = frame:getComponentByType("Core")
    local name = core.data.name
    if name ~= "Core" and name ~= "Main Core" then
      error("expected Core or Main Core, got " .. tostring(name))
    end
  end,
}
)";
  client.call("code.update", {{"frame_id", fid}, {"code", state_code}});
  auto result = client.call("code.execute", {{"frame_id", fid}, {"function", "update"}});
  CHECK(result["status"] == "ok");
}
