#include <rpc/handlers/frame_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <rpc/dto.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/well_known_entities.hpp>

namespace rpc {

void registerFrameHandlers(Server& server) {
  // frame.list — list all frames (summary)
  server.router().on("frame.list", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw std::runtime_error("Game not started");
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    nlohmann::json frames = nlohmann::json::array();
    auto view = registry.view<Frame>();
    for (auto entity : view) {
      auto& frame = view.get<Frame>(entity);
      frames.push_back(serializeFrameSummary(entity, frame));
    }
    return {{"frames", frames}};
  });

  // frame.get — get single frame by entity id
  server.router().on("frame.get", [](const Context& /*ctx*/, const nlohmann::json& params) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw std::runtime_error("Game not started");
    }

    if (!params.contains("id")) {
      throw std::runtime_error("Missing required parameter: id");
    }
    int entityId = params["id"].get<int>();
    auto entity = static_cast<entt::entity>(entityId);

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    if (!registry.valid(entity) || !registry.all_of<Frame>(entity)) {
      throw std::runtime_error("Frame not found");
    }

    auto& frame = registry.get<Frame>(entity);
    auto j = serializeFrame(frame);
    j["entityId"] = entityId;

    // Include transform if available
    if (registry.all_of<wl::transform>(entity)) {
      auto& t = registry.get<wl::transform>(entity);
      j["transform"] = {
        {"x", t.position.x},
        {"y", t.position.y},
        {"scale", t.scale},
        {"rotation", t.rotation},
        {"layer", t.layer}
      };
    }

    return j;
  });

  // frame.create — create frame from blueprint
  server.router().on("frame.create", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw std::runtime_error("Game not started");
    }

    if (!params.contains("name")) {
      throw std::runtime_error("Missing required parameter: name");
    }
    std::string name = params["name"].get<std::string>();

    // Use enqueueCommand for thread safety — the result is returned asynchronously
    // but we return an ack immediately
    gm.enqueueCommand([&gm, name]() {
      gm.addFrame(name);
    });

    return {{"status", "queued"}, {"name", name}};
  });

  // frame.update — update frame attributes
  server.router().on("frame.update", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw std::runtime_error("Game not started");
    }

    if (!params.contains("id")) {
      throw std::runtime_error("Missing required parameter: id");
    }
    int entityId = params["id"].get<int>();

    nlohmann::json attributes = nlohmann::json::object();
    if (params.contains("attributes")) {
      attributes = params["attributes"];
    }

    gm.enqueueCommand([&gm, entityId, attributes]() {
      std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
      auto& state = entt::locator<State>::value();
      auto& registry = state.registry;
      auto entity = static_cast<entt::entity>(entityId);

      if (!registry.valid(entity) || !registry.all_of<Frame>(entity)) {
        return; // Entity not found — silently ignore in queued command
      }

      auto& frame = registry.get<Frame>(entity);

      // Update name if provided
      if (attributes.contains("name") && attributes["name"].is_string()) {
        frame.data.name = attributes["name"].get<std::string>();
      }

      // Update description if provided
      if (attributes.contains("description") && attributes["description"].is_string()) {
        frame.data.description = attributes["description"].get<std::string>();
      }

      registry.replace<Frame>(entity, frame);
    });

    return {{"status", "queued"}, {"entityId", entityId}};
  });
}

} // namespace rpc
