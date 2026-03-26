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
  CHECK(amounts.contains("Ultralight"));
  CHECK(amounts.contains("Electronic Parts"));
  CHECK(amounts.contains("Science Packs"));
  CHECK(amounts.contains("Repair Packs"));

  // All values are numbers
  for (const auto& [k, v] : amounts.items()) {
    CHECK(v.is_number());
  }
}
