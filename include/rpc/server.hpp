#pragma once
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <ixwebsocket/IXWebSocketServer.h>
#include <rpc/router.hpp>
#include <liblog/liblog.hpp>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace rpc {

class Server {
public:
  Server(int port = 9800);
  ~Server();

  void start();
  void stop();

  Router& router() { return router_; }

  // Broadcast a notification to all connected clients
  void broadcast(const std::string& method, const nlohmann::json& params = {});

  // Broadcast to subscribers of a topic (or all if they subscribed with empty topics)
  void broadcastToSubscribers(const std::string& topic, const nlohmann::json& params);

  // Session management
  bool isClaimed() const;
  bool isOwner(const std::string& connectionId) const;
  bool claim(const std::string& connectionId);
  void release(const std::string& connectionId);

  // Subscription management
  void subscribe(const std::string& connId, const std::vector<std::string>& topics);
  void unsubscribe(const std::string& connId, const std::vector<std::string>& topics);

  int clientCount();
  std::string ownerConnectionId() const;
  int64_t uptimeMs() const;
  int port() const { return server_.getPort(); }

private:
  LibLog::Logger log_ = LibLog::Logger(fmt::color::cyan, "RPC");
  ix::WebSocketServer server_;
  Router router_;

  // Session ownership
  mutable std::mutex session_mutex_;
  std::string owner_connection_id_;
  std::chrono::steady_clock::time_point start_time_;

  // Subscriptions: topic -> set of connIds ("" = all topics)
  mutable std::mutex subscriptions_mutex_;
  std::map<std::string, std::set<std::string>> subscriptions_;

};

} // namespace rpc
