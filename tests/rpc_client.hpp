#pragma once
#include <nlohmann/json.hpp>
#include <ixwebsocket/IXWebSocket.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <stdexcept>

using json = nlohmann::json;

class RpcClient {
public:
  RpcClient() = default;
  ~RpcClient() { disconnect(); }

  void connect(const std::string& url,
               std::chrono::milliseconds timeout = std::chrono::seconds(5));
  void disconnect();

  json call(const std::string& method, const json& params = json::object(),
            std::chrono::milliseconds timeout = std::chrono::seconds(5));

  void notify(const std::string& method, const json& params = json::object());

  json call_raw(const std::string& method, const json& params = json::object(),
                std::chrono::milliseconds timeout = std::chrono::seconds(5));

  std::vector<json> drain_events();

  std::vector<json> wait_for_events(const std::string& method, int count = 1,
                                     std::chrono::milliseconds timeout = std::chrono::seconds(5));

  bool is_connected() const;

private:
  ix::WebSocket ws_;
  std::atomic<uint64_t> next_id_{1};

  // Pending responses keyed by id
  std::mutex resp_mutex_;
  std::condition_variable resp_cv_;
  std::map<uint64_t, json> responses_;

  // Incoming events (notifications = no id)
  std::mutex ev_mutex_;
  std::condition_variable ev_cv_;
  std::vector<json> events_;

  std::atomic<bool> connected_{false};
  std::mutex connect_mutex_;
  std::condition_variable connect_cv_;

  void send(const std::string& msg);
  json waitForResponse(uint64_t id, std::chrono::milliseconds timeout);
};
