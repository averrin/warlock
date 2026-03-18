#include <catch2/catch_test_macros.hpp>
#include <rpc/router.hpp>
#include <rpc/message.hpp>

using namespace rpc;
using json = nlohmann::json;

TEST_CASE("Router — dispatches to correct handler") {
  Router router;
  router.on("test.echo", [](const Context&, const json& params) -> json {
    return {{"echo", params}};
  });

  auto resp = json::parse(router.dispatch(
    R"({"jsonrpc":"2.0","id":1,"method":"test.echo","params":{"msg":"hello"}})"
  ));
  CHECK(resp["result"]["echo"]["msg"] == "hello");
}

TEST_CASE("Router — unknown method returns -32601") {
  Router router;
  auto resp = json::parse(router.dispatch(
    R"({"jsonrpc":"2.0","id":1,"method":"nope.nope","params":{}})"
  ));
  CHECK(resp["error"]["code"] == -32601);
}

TEST_CASE("Router — handler std::exception returns -32603") {
  Router router;
  router.on("boom.explode", [](const Context&, const json&) -> json {
    throw std::runtime_error("something broke");
  });

  auto resp = json::parse(router.dispatch(
    R"({"jsonrpc":"2.0","id":1,"method":"boom.explode","params":{}})"
  ));
  CHECK(resp["error"]["code"] == -32603);
}

TEST_CASE("Router — RpcError returns custom code") {
  Router router;
  router.on("custom.fail", [](const Context&, const json&) -> json {
    throw RpcError{1002, "not found"};
  });

  auto resp = json::parse(router.dispatch(
    R"({"jsonrpc":"2.0","id":1,"method":"custom.fail","params":{}})"
  ));
  CHECK(resp["error"]["code"] == 1002);
}

TEST_CASE("Router — notification (no id) returns empty string") {
  Router router;
  router.on("test.notify", [](const Context&, const json&) -> json {
    return {};
  });

  auto resp = router.dispatch(
    R"({"jsonrpc":"2.0","method":"test.notify","params":{}})"
  );
  CHECK(resp.empty());
}

TEST_CASE("Router — malformed JSON returns parse error") {
  Router router;
  auto resp = json::parse(router.dispatch("not json"));
  CHECK(resp["error"]["code"] == -32700);
}
