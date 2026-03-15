#pragma once
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <ixwebsocket/IXWebSocketServer.h>
#include <rpc/router.hpp>
#include <liblog/liblog.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

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

  // Session management
  bool isClaimed() const;
  bool isOwner(const std::string& connectionId) const;
  bool claim(const std::string& connectionId);
  void release(const std::string& connectionId);

  int clientCount();
  std::string ownerConnectionId() const;

private:
  LibLog::Logger log_ = LibLog::Logger(fmt::color::cyan, "RPC");
  ix::WebSocketServer server_;
  Router router_;

  // Session ownership
  mutable std::mutex session_mutex_;
  std::string owner_connection_id_;
};

} // namespace rpc
