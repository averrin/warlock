#pragma once
#include <rpc/message.hpp>
#include <rpc/server.hpp>
#include <stdexcept>

namespace rpc {

inline void requireClaim(const Server& server, const Context& ctx) {
  if (!server.isClaimed() || !server.isOwner(ctx.connectionId)) {
    throw rpc::RpcError{rpc::error::NOT_CLAIMED, "Session not claimed by this connection"};
  }
}

} // namespace rpc
