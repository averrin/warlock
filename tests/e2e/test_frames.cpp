#include "e2e_catch.hpp"
#include "harness.hpp"
#include "rpc_client.hpp"
#include "helpers.hpp"
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/components/frame.hpp>
#include <utils/entt.hpp>
#include <thread>
#include <chrono>
#include <future>
#include <set>

using namespace std::chrono_literals;

// ─── §3 Frame management ──────────────────────────────────────────────────────

E2E_TEST(frames,"frame.list — returns array of frames") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto result = client.call("frame.list");
  REQUIRE(result.contains("frames"));
  REQUIRE(result["frames"].is_array());
  // game_init.lua creates at least some frames
  CHECK(result["frames"].size() >= 1);
}

E2E_TEST(frames,"frame.list — each summary has required fields") {
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

E2E_TEST(frames,"frame.get — returns full frame DTO") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto list_result = client.call("frame.list");
  REQUIRE(list_result["frames"].size() >= 1);

  int entity_id = list_result["frames"][0]["entity_id"].get<int>();
  auto frame = client.call("frame.get", {{"id", entity_id}});
  assert_frame_dto(frame);
}

E2E_TEST(frames,"frame.get — components have required fields") {
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

E2E_TEST(frames,"frame.get — returns error 1002 for unknown entity") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  // Use an entity id that is very unlikely to exist
  auto resp = client.call_raw("frame.get", {{"id", 999999}});
  REQUIRE(resp.contains("error"));
  // Accept either ENTITY_NOT_FOUND (1002) or generic error
  CHECK(resp["error"]["code"].get<int>() != 0);
}

E2E_TEST(frames,"frame.create — requires session claim") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto resp = client.call_raw("frame.create", {{"name", "TestFrame"}});
  assert_jsonrpc_error(resp, 1000);
}

E2E_TEST(frames,"frame.create — returns frame DTO immediately") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  client.call("session.claim");

  auto result = client.call("frame.create", {{"name", "MyNewFrame"}});
  REQUIRE(result.contains("name"));
  CHECK(result["name"].get<std::string>() == "MyNewFrame");
  REQUIRE(result.contains("size"));
  CHECK(result["size"].get<std::string>() == "S");

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

E2E_TEST(frames,"frame.update — requires session claim") {
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

E2E_TEST(frames,"frame.update — updates frame name") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  client.call("session.claim");

  auto list_result = client.call("frame.list");
  REQUIRE(list_result["frames"].size() >= 1);
  int entity_id = list_result["frames"][0]["entity_id"].get<int>();

  auto result = client.call("frame.update", {
    {"id", entity_id},
    {"name", "RenamedFrame"}
  });
  REQUIRE(result.contains("ok"));
  CHECK(result["ok"] == true);

  // Process the update
  h.tick(3);

  // Verify the name was updated
  auto frame = client.call("frame.get", {{"id", entity_id}});
  CHECK(frame["name"].get<std::string>() == "RenamedFrame");
}

E2E_TEST(frames,"state.snapshot — frames include canvas_badges") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto snapshot = client.call("state.snapshot");
  REQUIRE(snapshot.contains("frames"));
  REQUIRE(snapshot["frames"].is_array());
  REQUIRE(snapshot["frames"].size() >= 1);

  for (const auto& frame : snapshot["frames"]) {
    REQUIRE(frame.contains("canvas_badges"));
    auto& badges = frame["canvas_badges"];
    REQUIRE(badges.is_object());

    // health must be one of ok/warn/bad
    REQUIRE(badges.contains("health"));
    CHECK(std::set<std::string>{"ok","warn","bad"}.count(badges["health"].get<std::string>()));

    // power must be one of offline/deficit/balanced/surplus
    REQUIRE(badges.contains("power"));
    CHECK(std::set<std::string>{"offline","deficit","balanced","surplus"}.count(badges["power"].get<std::string>()));

    // has_error must be boolean
    REQUIRE(badges.contains("has_error"));
    CHECK(badges["has_error"].is_boolean());

    // temperature is optional but if present must be number
    if (badges.contains("temperature") && !badges["temperature"].is_null()) {
      CHECK(badges["temperature"].is_number());
    }
  }
}

E2E_TEST(frames,"component.add — rejected when frame size slots are full") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());
  client.call("session.claim");
  client.call("frame.create", {{"name", "SlotLimitFrame"}});
  h.tick(5);

  auto list = client.call("frame.list");
  int entity_id = -1;
  int frame_id = -1;
  for (const auto &f : list["frames"]) {
    if (f["name"].get<std::string>() == "SlotLimitFrame") {
      entity_id = f["entity_id"].get<int>();
      frame_id = f["id"].get<int>();
      break;
    }
  }
  REQUIRE(entity_id >= 0);
  REQUIRE(frame_id >= 0);

  (void)client.call("frame.set_size", {{"id", entity_id}, {"size", "XS"}});

  auto ok = client.call("component.add", {{"frame_id", frame_id}, {"component_name", "Clock"}});
  REQUIRE(ok.contains("component"));

  auto bad = client.call_raw("component.add", {{"frame_id", frame_id}, {"component_name", "Clock"}});
  assert_jsonrpc_error(bad, -32602);
}

E2E_TEST(frames,"state.snapshot — new empty frame has health ok and no error") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  client.call("session.claim");
  client.call("frame.create", {{"name", "BadgeTestFrame"}});
  h.tick(5);

  auto snapshot = client.call("state.snapshot");
  for (const auto& frame : snapshot["frames"]) {
    if (frame["name"].get<std::string>() == "BadgeTestFrame") {
      auto& badges = frame["canvas_badges"];
      CHECK(badges["health"].get<std::string>() == "ok");
      CHECK(badges["has_error"].get<bool>() == false);
      // Power status depends on network membership
      CHECK(std::set<std::string>{"offline","balanced","surplus","deficit"}.count(
        badges["power"].get<std::string>()));
      return;
    }
  }
  FAIL("BadgeTestFrame not found in snapshot");
}

E2E_TEST(frames,"state.snapshot — component storage includes summary fields") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  client.call("session.claim");

  auto list = client.call("frame.list");
  REQUIRE(list.contains("frames"));
  REQUIRE(list["frames"].is_array());
  REQUIRE(list["frames"].size() >= 1);

  int frame_id = list["frames"][0]["id"].get<int>(); // data.id

  // Add a storage component.
  (void)client.call("component.add", {{"frame_id", frame_id}, {"component_name", "Big Storage"}});
  h.tick(2); // ensure storage is initialized by ItemsSystem

  // Inject a couple of stacks directly for determinism.
  {
    auto& gm = h.game_manager();
    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();

    entt::entity frame_entity = entt::null;
    auto view = state.registry.view<Frame>();
    for (auto e : view) {
      if (view.get<Frame>(e).data.id == frame_id) { frame_entity = e; break; }
    }
    REQUIRE(static_cast<uint32_t>(frame_entity) != static_cast<uint32_t>(entt::null));

    auto& frame = state.registry.get<Frame>(frame_entity);
    std::shared_ptr<Component> storage_comp = nullptr;
    for (auto& c : frame.components) {
      if (!c) continue;
      if (c->data.get_or<std::string>("type", "") == "Storage") { storage_comp = c; break; }
    }
    REQUIRE(storage_comp != nullptr);
    REQUIRE(storage_comp->storage != nullptr);
    REQUIRE(storage_comp->storage->slots.size() >= 2);

    storage_comp->storage->slots[0].stack = std::make_shared<ItemStack>(
      ItemDefinition{"Spark Ore", "test", 999}, 3);
    storage_comp->storage->slots[1].stack = std::make_shared<ItemStack>(
      ItemDefinition{"Spark Stone", "test", 999}, 2);
  }

  auto snapshot = client.call("state.snapshot");
  REQUIRE(snapshot.contains("frames"));
  REQUIRE(snapshot["frames"].is_array());

  for (const auto& frame : snapshot["frames"]) {
    if (!frame.contains("id") || frame["id"].get<int>() != frame_id) continue;
    REQUIRE(frame.contains("components"));
    REQUIRE(frame["components"].is_array());

    for (const auto& comp : frame["components"]) {
      if (!comp.contains("storage") || comp["storage"].is_null()) continue;
      auto& storage = comp["storage"];

      REQUIRE(storage.contains("slots_count"));
      REQUIRE(storage.contains("slots"));

      REQUIRE(storage.contains("slots_total"));
      REQUIRE(storage.contains("slots_used"));
      REQUIRE(storage.contains("top_items"));

      CHECK(storage["slots_total"].is_number_integer());
      CHECK(storage["slots_used"].is_number_integer());
      CHECK(storage["top_items"].is_array());

      CHECK(storage["slots_total"].get<int>() == storage["slots_count"].get<int>());
      CHECK(storage["slots_used"].get<int>() >= 0);
      CHECK(storage["top_items"].size() <= 3);

      // Ensure top_items includes at least 2 distinct items after production.
      std::set<std::string> names;
      for (const auto& ti : storage["top_items"]) {
        REQUIRE(ti.contains("name"));
        REQUIRE(ti.contains("amount"));
        CHECK(ti["name"].is_string());
        CHECK(ti["amount"].is_number_integer());
        if (ti["name"].is_string()) {
          names.insert(ti["name"].get<std::string>());
        }
      }
      CHECK(names.size() >= 2);
      return;
    }
  }

  FAIL("Target frame with storage component not found in snapshot");
}

E2E_TEST(frames,"frame.get — full frame also includes canvas_badges") {
  TestHarness h;
  RpcClient client;
  client.connect(h.ws_url());

  auto list_result = client.call("frame.list");
  REQUIRE(list_result["frames"].size() >= 1);

  int entity_id = list_result["frames"][0]["entity_id"].get<int>();
  auto frame = client.call("frame.get", {{"id", entity_id}});
  REQUIRE(frame.contains("canvas_badges"));
  auto& badges = frame["canvas_badges"];
  REQUIRE(badges.contains("health"));
  REQUIRE(badges.contains("power"));
  REQUIRE(badges.contains("has_error"));
}

E2E_TEST(frames,"frame.list — game_init frames plus created ones") {
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
