#include <catch2/catch_test_macros.hpp>
#include "harness.hpp"
#include "rpc_client.hpp"

#include <string>

TEST_CASE("items.list — returns non-empty, sorted, and has name/id") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  // Ensure systems have ticked at least once (items are loaded during gm.start)
  h.tick(1);

  auto result = client.call("items.list");
  REQUIRE(result.contains("items"));
  REQUIRE(result["items"].is_array());
  REQUIRE(result["items"].size() > 0);

  std::string prev;
  bool first = true;
  for (const auto& item : result["items"]) {
    REQUIRE(item.contains("name"));
    REQUIRE(item["name"].is_string());
    REQUIRE(item.contains("id"));
    REQUIRE(item["id"].is_string());

    std::string name = item["name"].get<std::string>();
    std::string id = item["id"].get<std::string>();
    REQUIRE(id == name); // v1 contract

    if (!first) {
      CHECK(prev <= name);
    }
    first = false;
    prev = name;
  }
}

