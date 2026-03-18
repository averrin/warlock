#pragma once
#include <memory>
#include <string>
#include <thread>
#include <atomic>

// Forward declarations
namespace rpc { class Server; }
class GameManager;

class TestHarness {
public:
  explicit TestHarness(int port = 0);  // port=0: auto-assign
  ~TestHarness();

  int port() const { return port_; }
  std::string ws_url() const { return "ws://127.0.0.1:" + std::to_string(port_); }

  // Run N game ticks synchronously
  void tick(int n = 1);

  GameManager& game_manager();
  rpc::Server& rpc_server();

private:
  int port_;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
