#pragma once
#include <nlohmann/json.hpp>
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <string>
using json = nlohmann::json;

inline void assert_frame_dto(const json& frame) {
  REQUIRE(frame.contains("id"));
  REQUIRE(frame["id"].is_number_integer());
  REQUIRE(frame.contains("entity_id"));
  REQUIRE(frame.contains("name"));
  REQUIRE(frame["name"].is_string());
  REQUIRE(frame.contains("size"));
  CHECK(std::set<std::string>{"S","M","L","G"}.count(frame["size"].get<std::string>()));
  REQUIRE(frame.contains("material"));
  REQUIRE(frame.contains("position"));
  REQUIRE(frame["position"].contains("x"));
  REQUIRE(frame["position"].contains("y"));
  REQUIRE(frame.contains("temperature"));
  REQUIRE(frame.contains("components"));
  REQUIRE(frame["components"].is_array());
  REQUIRE(frame.contains("effects"));
}

inline void assert_component_dto(const json& comp) {
  REQUIRE(comp.contains("id"));
  REQUIRE(comp.contains("name"));
  REQUIRE(comp.contains("type"));
  REQUIRE(comp.contains("state"));
  CHECK(std::set<std::string>{
    "DEACTIVATED","ACTIVATING","ACTIVE","DEACTIVATING",
    "COMP_ERROR","DESTROYED","BLOCKED","BROKEN"
  }.count(comp["state"].get<std::string>()));
  REQUIRE(comp.contains("size"));
  REQUIRE(comp.contains("attributes"));
  REQUIRE(comp["attributes"].is_object());
}

inline void assert_connection_dto(const json& conn) {
  REQUIRE(conn.contains("id"));
  REQUIRE(conn.contains("source"));
  REQUIRE(conn.contains("target"));
  REQUIRE(conn.contains("type"));
  CHECK(std::set<std::string>{"POWER","DATA"}.count(conn["type"].get<std::string>()));
}

inline void assert_jsonrpc_error(const json& resp, int expected_code) {
  REQUIRE(resp.contains("error"));
  REQUIRE(resp["error"]["code"].get<int>() == expected_code);
}
