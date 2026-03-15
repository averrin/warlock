#pragma once
#include <rpc/server.hpp>
#include <stdexcept>

namespace rpc {

inline void requireClaim(const Server& server, const Context& ctx) {
  if (!server.isClaimed()) {
    throw std::runtime_error("No session claimed");
  }
  if (!server.isOwner(ctx.connectionId)) {
    throw std::runtime_error("Not the session owner");
  }
}

} // namespace rpc
