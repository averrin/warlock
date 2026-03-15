#include <rpc/server.hpp>

namespace rpc {

Server::Server(int port)
    : server_(port, "0.0.0.0") {
}

Server::~Server() {
  stop();
}

void Server::start() {
  server_.setOnClientMessageCallback(
      [this](std::shared_ptr<ix::ConnectionState> connectionState,
             ix::WebSocket& ws,
             const ix::WebSocketMessagePtr& msg) {

        if (msg->type == ix::WebSocketMessageType::Open) {
          log_.info("Client connected: {}", connectionState->getId());
        } else if (msg->type == ix::WebSocketMessageType::Close) {
          log_.info("Client disconnected: {}", connectionState->getId());
          // Release session if the owner disconnects
          release(connectionState->getId());
        } else if (msg->type == ix::WebSocketMessageType::Message) {
          log_.debug("Received: {}", msg->str);
          rpc::Context ctx{connectionState->getId()};
          auto response = router_.dispatch(msg->str, ctx);
          ws.send(response);
        } else if (msg->type == ix::WebSocketMessageType::Error) {
          log_.error("Connection error: {}", msg->errorInfo.reason);
        }
      });

  auto res = server_.listen();
  if (!res.first) {
    log_.error("Failed to start WebSocket server: {}", res.second);
    return;
  }

  server_.start();
  log_.info("WebSocket RPC server started on port {}", server_.getPort());
}

void Server::stop() {
  server_.stop();
  log_.info("WebSocket RPC server stopped");
}

void Server::broadcast(const std::string& method, const nlohmann::json& params) {
  Notification notif;
  notif.method = method;
  notif.params = params;
  auto msg = notif.serialize();

  auto clients = server_.getClients();
  for (auto& client : clients) {
    client->send(msg);
  }
}

bool Server::isClaimed() const {
  std::lock_guard<std::mutex> lock(session_mutex_);
  return !owner_connection_id_.empty();
}

bool Server::isOwner(const std::string& connectionId) const {
  std::lock_guard<std::mutex> lock(session_mutex_);
  return owner_connection_id_ == connectionId;
}

bool Server::claim(const std::string& connectionId) {
  std::lock_guard<std::mutex> lock(session_mutex_);
  if (!owner_connection_id_.empty()) {
    return false;  // Already claimed
  }
  owner_connection_id_ = connectionId;
  return true;
}

void Server::release(const std::string& connectionId) {
  std::lock_guard<std::mutex> lock(session_mutex_);
  if (owner_connection_id_ == connectionId) {
    owner_connection_id_.clear();
  }
}

int Server::clientCount() {
  auto clients = server_.getClients();
  return static_cast<int>(clients.size());
}

std::string Server::ownerConnectionId() const {
  std::lock_guard<std::mutex> lock(session_mutex_);
  return owner_connection_id_;
}

} // namespace rpc
