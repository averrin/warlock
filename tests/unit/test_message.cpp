#include <catch2/catch_test_macros.hpp>
#include <rpc/message.hpp>

using namespace rpc;

TEST_CASE("Request::parse — valid request") {
  auto req = Request::parse(R"({"jsonrpc":"2.0","id":1,"method":"frame.list","params":{}})");
  REQUIRE(req.has_value());
  CHECK(req->id.has_value());
  CHECK(req->id.value() == 1);
  CHECK(req->method == "frame.list");
}

TEST_CASE("Request::parse — notification (no id)") {
  auto req = Request::parse(R"({"jsonrpc":"2.0","method":"input.hover","params":{"position":{"x":1,"y":2}}})");
  REQUIRE(req.has_value());
  CHECK(!req->id.has_value());
  CHECK(req->method == "input.hover");
}

TEST_CASE("Request::parse — malformed JSON") {
  auto req = Request::parse("not json");
  CHECK(!req.has_value());
}

TEST_CASE("Request::parse — missing method") {
  auto req = Request::parse(R"({"jsonrpc":"2.0","id":1})");
  CHECK(!req.has_value());
}

TEST_CASE("Request::parse — wrong jsonrpc version") {
  auto req = Request::parse(R"({"jsonrpc":"1.0","id":1,"method":"foo"})");
  CHECK(!req.has_value());
}

TEST_CASE("Response::serialize — correct shape") {
  Response r;
  r.id = 42;
  r.result = {{"ok", true}};
  auto j = nlohmann::json::parse(r.serialize());
  CHECK(j["jsonrpc"] == "2.0");
  CHECK(j["id"] == 42);
  CHECK(j["result"]["ok"] == true);
  CHECK(!j.contains("error"));
}

TEST_CASE("ErrorResponse::serialize — correct shape") {
  ErrorResponse e;
  e.id = 42;
  e.code = -32601;
  e.message = "Method not found";
  auto j = nlohmann::json::parse(e.serialize());
  CHECK(j["error"]["code"] == -32601);
  CHECK(j["error"]["message"] == "Method not found");
  CHECK(!j.contains("result"));
}

TEST_CASE("Notification::serialize — no id field") {
  Notification n;
  n.method = "event.test";
  n.params = {{"key", "val"}};
  auto j = nlohmann::json::parse(n.serialize());
  CHECK(!j.contains("id"));
  CHECK(j["method"] == "event.test");
  CHECK(j["params"]["key"] == "val");
}
