#include <catch2/catch_test_macros.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>
#include <sstream>
#include <utils/data/store.hpp>

// ---------------------------------------------------------------------------
// Store version handling tests
// ---------------------------------------------------------------------------

// Helper: serialize a Store to a binary string
static std::string storeToBytes(const Store &store) {
  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};
    ar(store);
  }
  return buf.str();
}

// Helper: deserialize a Store from a binary string
static void bytesToStore(const std::string &data, Store &store) {
  std::istringstream in(data);
  cereal::BinaryInputArchive ar{in};
  ar(store);
}

TEST_CASE("Store save writes expected values, not loaded values",
          "[store]") {
  // Create a store with version 2
  Store s(/*type=*/2, "test", "/tmp/test", /*version=*/2);
  s.initEmpty();

  auto data = storeToBytes(s);

  // Load into a store also expecting version 2
  Store loaded(2, "loaded", "/tmp/loaded", 2);
  REQUIRE_NOTHROW(bytesToStore(data, loaded));
  REQUIRE(loaded.name == "test");
  REQUIRE(loaded.file_version == 2);
}

TEST_CASE("Store loads older version files (version <= expected)",
          "[store]") {
  // Simulate an old file with version 1 by creating and saving with v1
  Store old_store(2, "old_data", "/tmp/old", 1);
  old_store.initEmpty();
  auto data = storeToBytes(old_store);

  // Load into a store expecting version 2
  Store new_store(2, "new", "/tmp/new", 2);
  REQUIRE_NOTHROW(bytesToStore(data, new_store));
  REQUIRE(new_store.file_version == 1);
  REQUIRE(new_store.name == "old_data");
}

TEST_CASE("Store rejects files with version newer than expected",
          "[store]") {
  // Create a file with version 3
  Store future_store(2, "future", "/tmp/future", 3);
  future_store.initEmpty();
  auto data = storeToBytes(future_store);

  // Try to load into a store expecting version 2
  Store current_store(2, "current", "/tmp/current", 2);
  REQUIRE_THROWS_AS(bytesToStore(data, current_store), std::runtime_error);
}

TEST_CASE("Store rejects mismatched type", "[store]") {
  Store s(2, "data", "/tmp/data", 1);
  s.initEmpty();
  auto data = storeToBytes(s);

  // Try loading into a store expecting type 3
  Store wrong_type(3, "other", "/tmp/other", 1);
  REQUIRE_THROWS_AS(bytesToStore(data, wrong_type), std::runtime_error);
}

TEST_CASE("Store rejects bad magic number", "[store]") {
  // Create raw bytes with wrong magic
  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};
    int8_t bad_magic = 0x00;
    int8_t type = 2;
    int8_t version = 1;
    std::string name = "bad";
    std::map<std::string, std::string> attrs;
    ar(bad_magic, type, version, name, attrs);
  }
  auto data = buf.str();

  Store s(2, "test", "/tmp/test", 1);
  REQUIRE_THROWS_AS(bytesToStore(data, s), std::runtime_error);
}

TEST_CASE("Store initEmpty sets correct values", "[store]") {
  Store s(5, "init_test", "/tmp/init", 3);
  REQUIRE(s.type == 0);
  REQUIRE(s.version == 0);

  s.initEmpty();
  REQUIRE(s.type == 5);
  REQUIRE(s.version == 3);
  REQUIRE(s.file_version == 3);
}

TEST_CASE("Store attributes survive round-trip", "[store]") {
  Store s(2, "attrs", "/tmp/attrs", 2);
  s.initEmpty();
  s.attributes["saved_at"] = "2026-01-01";
  s.attributes["author"] = "test";

  auto data = storeToBytes(s);

  Store loaded(2, "loaded", "/tmp/loaded", 2);
  REQUIRE_NOTHROW(bytesToStore(data, loaded));
  REQUIRE(loaded.attributes.size() == 2);
  REQUIRE(loaded.attributes["saved_at"] == "2026-01-01");
  REQUIRE(loaded.attributes["author"] == "test");
}

TEST_CASE("Store save always upgrades to expected version", "[store]") {
  // Simulate: load a v1 file, then re-save it — output should be v2
  Store v1(2, "data", "/tmp/data", 1);
  v1.initEmpty();
  auto v1_data = storeToBytes(v1);

  // Load v1 into a v2-expecting store
  Store loaded(2, "loaded", "/tmp/loaded", 2);
  bytesToStore(v1_data, loaded);
  REQUIRE(loaded.file_version == 1);

  // Re-save — should write version 2
  auto v2_data = storeToBytes(loaded);

  // Load the re-saved data — should now be version 2
  Store reloaded(2, "reloaded", "/tmp/reloaded", 2);
  bytesToStore(v2_data, reloaded);
  REQUIRE(reloaded.file_version == 2);
}
