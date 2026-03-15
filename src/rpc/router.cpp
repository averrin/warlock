#include <rpc/router.hpp>
#include <stdexcept>

namespace rpc {

void Router::on(const std::string& method, Handler handler) {
  handlers_[method] = std::move(handler);
}

std::string Router::dispatch(const std::string& raw_message, const Context& ctx) {
  auto req = Request::parse(raw_message);

  if (!req.has_value()) {
    // Could be a parse error or invalid request
    ErrorResponse err;
    err.id = nullptr;
    err.code = error::PARSE_ERROR;
    err.message = "Parse error";
    return err.serialize();
  }

  auto it = handlers_.find(req->method);
  if (it == handlers_.end()) {
    ErrorResponse err;
    err.id = req->id;
    err.code = error::METHOD_NOT_FOUND;
    err.message = "Method not found: " + req->method;
    return err.serialize();
  }

  try {
    auto result = it->second(ctx, req->params);
    Response resp;
    resp.id = req->id;
    resp.result = std::move(result);
    return resp.serialize();
  } catch (const nlohmann::json::exception& e) {
    ErrorResponse err;
    err.id = req->id;
    err.code = error::INVALID_PARAMS;
    err.message = std::string("Invalid params: ") + e.what();
    return err.serialize();
  } catch (const std::exception& e) {
    ErrorResponse err;
    err.id = req->id;
    err.code = error::INTERNAL_ERROR;
    err.message = e.what();
    return err.serialize();
  }
}

} // namespace rpc
