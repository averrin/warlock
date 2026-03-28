#include "e2e_catch.hpp"
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Helper: find the Economy entity in the entities.list result
static json find_economy_entity(RpcClient& client) {
  auto result = client.call("entities.list");
  REQUIRE(result.contains("entities"));
  for (const auto& e : result["entities"]) {
    if (e.value("label", "") == "Economy") return e;
  }
  return json{};
}

E2E_TEST(economy, "entities.list — Economy entity has SpendablePool component") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto econ = find_economy_entity(client);
  REQUIRE(!econ.is_null());
  REQUIRE(econ.contains("components"));
  REQUIRE(econ["components"].contains("SpendablePool"));

  const auto& sp = econ["components"]["SpendablePool"];
  REQUIRE(sp.contains("amounts"));
  REQUIRE(sp["amounts"].is_object());
}

E2E_TEST(economy, "entities.list — SpendablePool amounts contain expected keys") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto econ = find_economy_entity(client);
  REQUIRE(!econ.is_null());
  const auto& amounts = econ["components"]["SpendablePool"]["amounts"];

  // Keys defined in config.lua spendable_pool table
  CHECK(amounts.contains("Frame Parts"));
  CHECK(amounts.contains("Ultralight Structures"));
  CHECK(amounts.contains("Electronic Parts"));
  CHECK(amounts.contains("Science Packs"));
  CHECK(amounts.contains("Repair Packs"));

  // All values are numbers
  for (const auto& [k, v] : amounts.items()) {
    CHECK(v.is_number());
  }
}

E2E_TEST(economy, "entities.set_field — SpendablePool amount can be updated") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");

  // Find the Economy entity id
  auto result = client.call("entities.list");
  int econ_id = -1;
  for (const auto& e : result["entities"]) {
    if (e.value("label", "") == "Economy") {
      econ_id = e["entity_id"].get<int>();
      break;
    }
  }
  REQUIRE(econ_id != -1);

  // Set "Frame Parts" to a known value
  auto set_result = client.call("entities.set_field", {
    {"entity_id", econ_id},
    {"component", "SpendablePool"},
    {"field", "Frame Parts"},
    {"value", 42}
  });
  REQUIRE(set_result.contains("ok"));
  CHECK(set_result["ok"] == true);

  // Verify the mutation persisted
  auto updated = client.call("entities.list");
  for (const auto& e : updated["entities"]) {
    if (e["entity_id"].get<int>() == econ_id) {
      const auto& amounts = e["components"]["SpendablePool"]["amounts"];
      CHECK(amounts["Frame Parts"].get<int64_t>() == 42);
      break;
    }
  }
}

E2E_TEST(economy, "entities.set_field — SpendablePool rejects removal attempt") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");

  auto result = client.call("entities.list");
  int econ_id = -1;
  for (const auto& e : result["entities"]) {
    if (e.value("label", "") == "Economy") {
      econ_id = e["entity_id"].get<int>();
      break;
    }
  }
  REQUIRE(econ_id != -1);

  // Attempt to remove the SpendablePool component — should throw
  bool threw = false;
  try {
    client.call("entities.component.remove", {
      {"entity_id", econ_id},
      {"component", "SpendablePool"}
    });
  } catch (...) {
    threw = true;
  }
  CHECK(threw);
}
