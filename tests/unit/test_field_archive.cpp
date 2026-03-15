#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <sstream>
#include <utils/data/field_archive.hpp>

// ---------------------------------------------------------------------------
// FieldOutputArchive / FieldInputArchive unit tests
// ---------------------------------------------------------------------------

TEST_CASE("FieldArchive round-trip for basic types", "[field_archive]") {
  SECTION("int field") {
    FieldOutputArchive foa;
    foa.field("x", 42);

    std::ostringstream buf;
    {
      cereal::BinaryOutputArchive ar{buf};
      foa.writeTo(ar);
    }

    FieldInputArchive fia;
    {
      std::istringstream in(buf.str());
      cereal::BinaryInputArchive ar{in};
      fia.readFrom(ar);
    }

    int x = 0;
    REQUIRE(fia.field("x", x));
    REQUIRE(x == 42);
  }

  SECTION("float field") {
    FieldOutputArchive foa;
    foa.field("val", 3.14f);

    std::ostringstream buf;
    {
      cereal::BinaryOutputArchive ar{buf};
      foa.writeTo(ar);
    }

    FieldInputArchive fia;
    {
      std::istringstream in(buf.str());
      cereal::BinaryInputArchive ar{in};
      fia.readFrom(ar);
    }

    float val = 0.0f;
    REQUIRE(fia.field("val", val));
    REQUIRE_THAT(val, Catch::Matchers::WithinRel(3.14f, 0.001f));
  }

  SECTION("string field") {
    FieldOutputArchive foa;
    foa.field("name", std::string("hello"));

    std::ostringstream buf;
    {
      cereal::BinaryOutputArchive ar{buf};
      foa.writeTo(ar);
    }

    FieldInputArchive fia;
    {
      std::istringstream in(buf.str());
      cereal::BinaryInputArchive ar{in};
      fia.readFrom(ar);
    }

    std::string name;
    REQUIRE(fia.field("name", name));
    REQUIRE(name == "hello");
  }

  SECTION("bool field") {
    FieldOutputArchive foa;
    foa.field("flag", true);

    std::ostringstream buf;
    {
      cereal::BinaryOutputArchive ar{buf};
      foa.writeTo(ar);
    }

    FieldInputArchive fia;
    {
      std::istringstream in(buf.str());
      cereal::BinaryInputArchive ar{in};
      fia.readFrom(ar);
    }

    bool flag = false;
    REQUIRE(fia.field("flag", flag));
    REQUIRE(flag == true);
  }
}

TEST_CASE("FieldArchive round-trip for multiple fields", "[field_archive]") {
  FieldOutputArchive foa;
  foa.field("x", 10);
  foa.field("y", 20);
  foa.field("name", std::string("entity_1"));
  foa.field("active", true);

  REQUIRE(foa.size() == 4);

  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};
    foa.writeTo(ar);
  }

  FieldInputArchive fia;
  {
    std::istringstream in(buf.str());
    cereal::BinaryInputArchive ar{in};
    fia.readFrom(ar);
  }

  REQUIRE(fia.fieldCount() == 4);

  int x = 0, y = 0;
  std::string name;
  bool active = false;
  REQUIRE(fia.field("x", x));
  REQUIRE(fia.field("y", y));
  REQUIRE(fia.field("name", name));
  REQUIRE(fia.field("active", active));
  REQUIRE(x == 10);
  REQUIRE(y == 20);
  REQUIRE(name == "entity_1");
  REQUIRE(active == true);
}

TEST_CASE("FieldInputArchive: missing field keeps default", "[field_archive]") {
  // Write only "x"
  FieldOutputArchive foa;
  foa.field("x", 99);

  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};
    foa.writeTo(ar);
  }

  FieldInputArchive fia;
  {
    std::istringstream in(buf.str());
    cereal::BinaryInputArchive ar{in};
    fia.readFrom(ar);
  }

  // "y" was never written — field() should return false and leave default
  int y = 42;
  REQUIRE_FALSE(fia.field("y", y));
  REQUIRE(y == 42); // unchanged
}

TEST_CASE("FieldInputArchive: unknown fields are silently skipped",
          "[field_archive]") {
  // Write "x" and "obsolete"
  FieldOutputArchive foa;
  foa.field("x", 1);
  foa.field("obsolete", std::string("old_data"));
  foa.field("y", 2);

  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};
    foa.writeTo(ar);
  }

  FieldInputArchive fia;
  {
    std::istringstream in(buf.str());
    cereal::BinaryInputArchive ar{in};
    fia.readFrom(ar);
  }

  // Only read x and y, skip obsolete
  int x = 0, y = 0;
  REQUIRE(fia.field("x", x));
  REQUIRE(fia.field("y", y));
  REQUIRE(x == 1);
  REQUIRE(y == 2);
  // "obsolete" is present but we just don't read it — that's fine
  REQUIRE(fia.has("obsolete"));
}

TEST_CASE("FieldInputArchive::has() checks presence", "[field_archive]") {
  FieldOutputArchive foa;
  foa.field("exists", 1);

  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};
    foa.writeTo(ar);
  }

  FieldInputArchive fia;
  {
    std::istringstream in(buf.str());
    cereal::BinaryInputArchive ar{in};
    fia.readFrom(ar);
  }

  REQUIRE(fia.has("exists"));
  REQUIRE_FALSE(fia.has("missing"));
}

TEST_CASE("FieldArchive round-trip for vector", "[field_archive]") {
  std::vector<std::string> tags = {"fire", "magic", "rare"};

  FieldOutputArchive foa;
  foa.field("tags", tags);

  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};
    foa.writeTo(ar);
  }

  FieldInputArchive fia;
  {
    std::istringstream in(buf.str());
    cereal::BinaryInputArchive ar{in};
    fia.readFrom(ar);
  }

  std::vector<std::string> loaded_tags;
  REQUIRE(fia.field("tags", loaded_tags));
  REQUIRE(loaded_tags == tags);
}

TEST_CASE("FieldArchive empty archive", "[field_archive]") {
  FieldOutputArchive foa;
  REQUIRE(foa.size() == 0);

  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};
    foa.writeTo(ar);
  }

  FieldInputArchive fia;
  {
    std::istringstream in(buf.str());
    cereal::BinaryInputArchive ar{in};
    fia.readFrom(ar);
  }

  REQUIRE(fia.fieldCount() == 0);
}

TEST_CASE("FIELD macro works correctly", "[field_archive]") {
  int health = 100;
  std::string name = "warrior";

  FieldOutputArchive foa;
  FIELD(foa, health);
  FIELD(foa, name);

  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};
    foa.writeTo(ar);
  }

  FieldInputArchive fia;
  {
    std::istringstream in(buf.str());
    cereal::BinaryInputArchive ar{in};
    fia.readFrom(ar);
  }

  // FIELD macro uses the variable name as the field name
  REQUIRE(fia.has("health"));
  REQUIRE(fia.has("name"));

  int loaded_health = 0;
  std::string loaded_name;
  FIELD(fia, loaded_health);   // reads "loaded_health" — won't match "health"
  REQUIRE(loaded_health == 0); // no match because field name differs

  // Use field() directly with the correct name
  REQUIRE(fia.field("health", loaded_health));
  REQUIRE(loaded_health == 100);
  REQUIRE(fia.field("name", loaded_name));
  REQUIRE(loaded_name == "warrior");
}

// ---------------------------------------------------------------------------
// Test forward/backward compatibility scenario
// ---------------------------------------------------------------------------
TEST_CASE("Forward compat: new code reads old data with fewer fields",
          "[field_archive]") {
  // Simulate "old" version that only wrote x and y
  FieldOutputArchive foa;
  foa.field("x", 5);
  foa.field("y", 10);

  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};
    foa.writeTo(ar);
  }

  // "New" code expects x, y, and z (new field)
  FieldInputArchive fia;
  {
    std::istringstream in(buf.str());
    cereal::BinaryInputArchive ar{in};
    fia.readFrom(ar);
  }

  int x = 0, y = 0, z = 99; // z has default
  REQUIRE(fia.field("x", x));
  REQUIRE(fia.field("y", y));
  REQUIRE_FALSE(fia.field("z", z));
  REQUIRE(x == 5);
  REQUIRE(y == 10);
  REQUIRE(z == 99); // kept default
}

TEST_CASE("Backward compat: old code reads new data with extra fields",
          "[field_archive]") {
  // "New" version wrote x, y, z, and w
  FieldOutputArchive foa;
  foa.field("x", 1);
  foa.field("y", 2);
  foa.field("z", 3);
  foa.field("w", 4);

  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};
    foa.writeTo(ar);
  }

  // "Old" code only knows about x and y
  FieldInputArchive fia;
  {
    std::istringstream in(buf.str());
    cereal::BinaryInputArchive ar{in};
    fia.readFrom(ar);
  }

  int x = 0, y = 0;
  REQUIRE(fia.field("x", x));
  REQUIRE(fia.field("y", y));
  REQUIRE(x == 1);
  REQUIRE(y == 2);
  // z and w are just ignored — no crash, no error
}

// ---------------------------------------------------------------------------
// Test enum serialization via FieldArchive
// ---------------------------------------------------------------------------
enum class TestEnum : int { A = 0, B = 1, C = 2 };

TEST_CASE("FieldArchive round-trip for enum", "[field_archive]") {
  FieldOutputArchive foa;
  foa.field("mode", TestEnum::B);

  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};
    foa.writeTo(ar);
  }

  FieldInputArchive fia;
  {
    std::istringstream in(buf.str());
    cereal::BinaryInputArchive ar{in};
    fia.readFrom(ar);
  }

  TestEnum mode = TestEnum::A;
  REQUIRE(fia.field("mode", mode));
  REQUIRE(mode == TestEnum::B);
}

// ---------------------------------------------------------------------------
// Test multiple sequential writeTo/readFrom on the same stream
// ---------------------------------------------------------------------------
TEST_CASE("Multiple FieldArchives in sequence on one stream",
          "[field_archive]") {
  std::ostringstream buf;
  {
    cereal::BinaryOutputArchive ar{buf};

    FieldOutputArchive foa1;
    foa1.field("a", 1);
    foa1.writeTo(ar);

    FieldOutputArchive foa2;
    foa2.field("b", 2);
    foa2.field("c", 3);
    foa2.writeTo(ar);
  }

  std::istringstream in(buf.str());
  cereal::BinaryInputArchive ar{in};

  FieldInputArchive fia1;
  fia1.readFrom(ar);
  int a = 0;
  REQUIRE(fia1.field("a", a));
  REQUIRE(a == 1);

  FieldInputArchive fia2;
  fia2.readFrom(ar);
  int b = 0, c = 0;
  REQUIRE(fia2.field("b", b));
  REQUIRE(fia2.field("c", c));
  REQUIRE(b == 2);
  REQUIRE(c == 3);
}
