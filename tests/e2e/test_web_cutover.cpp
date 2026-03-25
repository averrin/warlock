#include "e2e_catch.hpp"
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"

E2E_TEST(web,"web cutover: frame.move updates frame position") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");

  auto createResult = client.call("frame.create", {{"name", "MoveTarget"}});
  REQUIRE(createResult.contains("status"));
  CHECK(createResult["status"] == "queued");

  h.tick(3);

  // Find the created frame
  auto list = client.call("frame.list");
  int entityId = -1;
  for (const auto& f : list["frames"]) {
    if (f["name"].get<std::string>() == "MoveTarget") {
      entityId = f["entity_id"].get<int>();
      break;
    }
  }
  REQUIRE(entityId != -1);

  auto moveResult =
      client.call("frame.move", {{"id", entityId}, {"position", {{"x", 256}, {"y", 384}}}});
  REQUIRE(moveResult.contains("ok"));
  CHECK(moveResult["ok"] == true);

  auto frameResult = client.call("frame.get", {{"id", entityId}});
  REQUIRE(frameResult.contains("position"));
  CHECK(frameResult["position"]["x"].get<int>() == 256);
  CHECK(frameResult["position"]["y"].get<int>() == 384);
}

E2E_TEST(web,"web cutover: required read methods return expected shape") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto gameState = client.call("game.state");
  REQUIRE(gameState.contains("frames"));
  REQUIRE(gameState.contains("connections"));
  REQUIRE(gameState.contains("started"));

  auto power = client.call("power.networks");
  REQUIRE(power.contains("networks"));
  REQUIRE(power["networks"].is_array());
  if (!power["networks"].empty()) {
    const auto& net = power["networks"][0];
    REQUIRE(net.contains("production"));
    REQUIRE(net.contains("consumption"));
    REQUIRE(net.contains("accumulated"));
    REQUIRE(net.contains("history"));
    REQUIRE(net["history"].is_object());
    REQUIRE(net["history"].contains("production"));
    REQUIRE(net["history"].contains("total"));
    REQUIRE(net["history"].contains("consumption"));
    CHECK(net["history"]["production"].is_array());
    CHECK(net["history"]["total"].is_array());
    CHECK(net["history"]["consumption"].is_array());
  }

  auto envStatusRaw = client.call_raw("env.status");
  if (envStatusRaw.contains("error")) {
    CHECK(envStatusRaw["error"]["code"].get<int>() != 0);
  } else {
    REQUIRE(envStatusRaw.contains("result"));
    REQUIRE(envStatusRaw["result"].contains("env"));
  }
}

E2E_TEST(web,"web cutover: game speed control API is wired") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");

  auto setSpeed = client.call("game.speed.set", {{"multiplier", 5}});
  REQUIRE(setSpeed.contains("multiplier"));
  CHECK(setSpeed["multiplier"].get<float>() == 5.0f);

  auto speed = client.call("game.speed.get");
  REQUIRE(speed.contains("paused"));
  REQUIRE(speed.contains("multiplier"));
  CHECK(speed["paused"].get<bool>() == true);
  CHECK(speed["multiplier"].get<float>() == 5.0f);

  client.call("game.resume");
  auto resumed = client.call("game.speed.get");
  CHECK(resumed["paused"].get<bool>() == false);

  client.call("game.pause");
  auto paused = client.call("game.speed.get");
  CHECK(paused["paused"].get<bool>() == true);
}

E2E_TEST(web,"web cutover: env.status exposes chart history keys") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  h.tick(8);

  auto result = client.call("env.status");
  REQUIRE(result.contains("env"));
  REQUIRE(result["env"].contains("history"));

  const auto& history = result["env"]["history"];
  REQUIRE(history.contains("temperature"));
  REQUIRE(history.contains("air_flow"));
  REQUIRE(history.contains("sun"));
  CHECK(history["temperature"].is_array());
  CHECK(history["air_flow"].is_array());
  CHECK(history["sun"].is_array());
}
