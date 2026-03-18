#include <rpc/router.hpp>
#include <stdexcept>

namespace rpc {

void Router::on(const std::string& method, Handler handler) {
  handlers_[method] = std::move(handler);
}

std::string Router::dispatch(const std::string& raw_message, const Context& ctx) {
  auto req = Request::parse(raw_message);

  if (!req.has_value()) {
    ErrorResponse err;
    err.id = nullptr;
    err.code = error::PARSE_ERROR;
    err.message = "Parse error";
    return err.serialize();
  }

  bool is_notification = !req->id.has_value();

  auto it = handlers_.find(req->method);
  if (it == handlers_.end()) {
    if (is_notification) return "";
    ErrorResponse err;
    err.id = req->id.value_or(nullptr);
    err.code = error::METHOD_NOT_FOUND;
    err.message = "Method not found: " + req->method;
    return err.serialize();
  }

  try {
    auto result = it->second(ctx, req->params);
    if (is_notification) return "";
    Response resp;
    resp.id = req->id.value_or(nullptr);
    resp.result = std::move(result);
    return resp.serialize();
  } catch (const RpcError& e) {
    if (is_notification) return "";
    ErrorResponse err;
    err.id = req->id.value_or(nullptr);
    err.code = e.code;
    err.message = e.message;
    return err.serialize();
  } catch (const nlohmann::json::exception& e) {
    if (is_notification) return "";
    ErrorResponse err;
    err.id = req->id.value_or(nullptr);
    err.code = error::INVALID_PARAMS;
    err.message = std::string("Invalid params: ") + e.what();
    return err.serialize();
  } catch (const std::exception& e) {
    if (is_notification) return "";
    ErrorResponse err;
    err.id = req->id.value_or(nullptr);
    err.code = error::INTERNAL_ERROR;
    err.message = e.what();
    return err.serialize();
  }
}

} // namespace rpc
