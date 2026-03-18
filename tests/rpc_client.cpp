#include "rpc_client.hpp"
#include <stdexcept>
#include <thread>
#include <ixwebsocket/IXNetSystem.h>

void RpcClient::connect(const std::string& url, std::chrono::milliseconds timeout) {
  ws_.setUrl(url);
  ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
    if (msg->type == ix::WebSocketMessageType::Open) {
      {
        std::lock_guard<std::mutex> lk(connect_mutex_);
        connected_ = true;
      }
      connect_cv_.notify_all();
    } else if (msg->type == ix::WebSocketMessageType::Close ||
               msg->type == ix::WebSocketMessageType::Error) {
      connected_ = false;
    } else if (msg->type == ix::WebSocketMessageType::Message) {
      try {
        auto j = json::parse(msg->str);
        if (j.contains("id") && !j["id"].is_null()) {
          // Response to a call
          uint64_t id = j["id"].get<uint64_t>();
          {
            std::lock_guard<std::mutex> lk(resp_mutex_);
            responses_[id] = j;
          }
          resp_cv_.notify_all();
        } else {
          // Notification / event
          {
            std::lock_guard<std::mutex> lk(ev_mutex_);
            events_.push_back(j);
          }
          ev_cv_.notify_all();
        }
      } catch (...) {}
    }
  });

  ws_.start();

  std::unique_lock<std::mutex> lk(connect_mutex_);
  if (!connect_cv_.wait_for(lk, timeout, [this]{ return connected_.load(); })) {
    ws_.stop();
    throw std::runtime_error("RpcClient: connect timeout");
  }
}

void RpcClient::disconnect() {
  ws_.stop();
  connected_ = false;
}

bool RpcClient::is_connected() const { return connected_; }

void RpcClient::send(const std::string& msg) {
  ws_.send(msg);
}

json RpcClient::waitForResponse(uint64_t id, std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lk(resp_mutex_);
  if (!resp_cv_.wait_for(lk, timeout, [&]{ return responses_.count(id) > 0; })) {
    throw std::runtime_error("RpcClient: timeout waiting for response to id=" + std::to_string(id));
  }
  auto resp = responses_.at(id);
  responses_.erase(id);
  return resp;
}

json RpcClient::call_raw(const std::string& method, const json& params,
                          std::chrono::milliseconds timeout) {
  uint64_t id = next_id_++;
  json req = {
    {"jsonrpc", "2.0"},
    {"id", id},
    {"method", method},
    {"params", params}
  };
  send(req.dump());
  return waitForResponse(id, timeout);
}

json RpcClient::call(const std::string& method, const json& params,
                     std::chrono::milliseconds timeout) {
  auto resp = call_raw(method, params, timeout);
  if (resp.contains("error")) {
    throw std::runtime_error("RPC error " +
      std::to_string(resp["error"]["code"].get<int>()) + ": " +
      resp["error"]["message"].get<std::string>());
  }
  return resp["result"];
}

void RpcClient::notify(const std::string& method, const json& params) {
  json req = {{"jsonrpc", "2.0"}, {"method", method}, {"params", params}};
  send(req.dump());
}

std::vector<json> RpcClient::drain_events() {
  std::lock_guard<std::mutex> lk(ev_mutex_);
  auto result = events_;
  events_.clear();
  return result;
}

std::vector<json> RpcClient::wait_for_events(const std::string& method, int count,
                                               std::chrono::milliseconds timeout) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  std::vector<json> matching;

  std::unique_lock<std::mutex> lk(ev_mutex_);
  while (matching.size() < static_cast<size_t>(count)) {
    if (!ev_cv_.wait_until(lk, deadline, [&]{
      for (auto& e : events_) {
        if (e.contains("method") && e["method"] == method) return true;
      }
      return false;
    })) break;

    // Collect matching events
    for (auto& e : events_) {
      if (e.contains("method") && e["method"] == method)
        matching.push_back(e);
    }
    events_.clear();
    if (matching.size() >= static_cast<size_t>(count)) break;
  }
  return matching;
}
