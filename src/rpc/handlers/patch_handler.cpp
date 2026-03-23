#include <rpc/handlers/patch_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <game/components/resource_patch.hpp>
#include <game/patch_loader.hpp>
#include <game/systems/patch_generation.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <utils/entt.hpp>
#include <utils/entt_draw.hpp>
#include <algorithm>
#include <fmt/core.h>
#include <random>

namespace rpc {

nlohmann::json serializePatch(entt::entity e, const ResourcePatch& patch, const hf::meta& meta) {
  nlohmann::json cells_arr = nlohmann::json::array();
  for (const auto& [x, y] : patch.cells) {
    cells_arr.push_back({x, y});
  }
  
  auto& loader = entt::locator<PatchLoader>::value();
  auto* def = loader.get_patch_type(patch.patch_type);
  
  nlohmann::json color = {{"r", 255}, {"g", 200}, {"b", 50}, {"a", 100}};
  if (def) {
    color = {{"r", def->color.r}, {"g", def->color.g}, {"b", def->color.b}, {"a", def->color.a}};
  }
  
  return {
    {"id", static_cast<int>(e)},
    {"name", meta.name},
    {"type", patch.patch_type},
    {"item", patch.item_name},
    {"obstacle", patch.obstacle},
    {"cells", cells_arr},
    {"bounds", {{"x", patch.min_x}, {"y", patch.min_y}, 
                {"w", patch.max_x - patch.min_x + 1}, 
                {"h", patch.max_y - patch.min_y + 1}}},
    {"color", color}
  };
}

void registerPatchHandlers(Server& server) {
  server.router().on("patches.list", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    nlohmann::json patches = nlohmann::json::array();
    
    for (auto e : state.registry.view<ResourcePatch>()) {
      auto& patch = state.registry.get<ResourcePatch>(e);
      auto& meta = state.registry.get<hf::meta>(e);
      patches.push_back(serializePatch(e, patch, meta));
    }
    
    return {{"patches", patches}};
  });

  server.router().on("patches.types", [](const Context& /*ctx*/, const nlohmann::json& /*params*/) -> nlohmann::json {
    auto& loader = entt::locator<PatchLoader>::value();
    nlohmann::json types = nlohmann::json::array();
    
    for (const auto& [key, def] : loader.get_patch_types()) {
      types.push_back({
        {"key", key},
        {"name", def.name},
        {"item", def.item},
        {"obstacle", def.obstacle},
        {"color", {{"r", def.color.r}, {"g", def.color.g}, 
                   {"b", def.color.b}, {"a", def.color.a}}},
        {"generation", {
          {"min_width", def.generation.min_width},
          {"max_width", def.generation.max_width},
          {"min_height", def.generation.min_height},
          {"max_height", def.generation.max_height},
        }},
      });
    }
    
    return {{"types", types}};
  });

  server.router().on("patches.create", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }

    std::string type = params.value("type", "sparkstone_deposit");
    int x = params.value("x", 0);
    int y = params.value("y", 0);
    
    auto& loader = entt::locator<PatchLoader>::value();
    auto* def = loader.get_patch_type(type);
    if (!def) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown patch type: " + type};
    }
    
    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    const int w0 = std::max(1, std::min(def->generation.min_width, def->generation.max_width));
    const int w1 = std::max(w0, std::max(def->generation.min_width, def->generation.max_width));
    const int h0 = std::max(1, std::min(def->generation.min_height, def->generation.max_height));
    const int h1 = std::max(h0, std::max(def->generation.min_height, def->generation.max_height));
    std::uniform_int_distribution<int> w_dis(w0, w1);
    std::uniform_int_distribution<int> h_dis(h0, h1);
    
    int width = w_dis(gen);
    int height = h_dis(gen);
    
    auto cells = generateBlob(width, height, def->generation.fill_probability, 
                              def->generation.smoothing_rounds);
    
    for (auto& [cx, cy] : cells) {
      cx += x;
      cy += y;
    }
    
    auto e = state.registry.create();
    
    hf::meta meta;
    meta.name = def->name;
    meta.id = fmt::format("PATCH-{}", static_cast<int>(e));
    state.registry.emplace<hf::meta>(e, meta);
    
    wl::transform transform;
    transform.position.x = static_cast<float>(x * 25);
    transform.position.y = static_cast<float>(y * 25);
    state.registry.emplace<wl::transform>(e, transform);
    
    ResourcePatch patch;
    patch.patch_type = type;
    patch.item_name = def->item;
    patch.obstacle = def->obstacle;
    patch.cells = cells;
    patch.recalculateBounds();
    state.registry.emplace<ResourcePatch>(e, patch);
    
    nlohmann::json result = {{"id", static_cast<int>(e)}, {"status", "created"}};
    logWebAction(server, "patches.create", "ok", {{"type", type}, {"id", static_cast<int>(e)}});
    return result;
  });

  server.router().on("patches.delete", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: id"};
    }
    int id = params["id"].get<int>();
    
    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto e = static_cast<entt::entity>(id);
    
    if (!state.registry.valid(e) || !state.registry.all_of<ResourcePatch>(e)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Patch not found"};
    }
    
    state.registry.destroy(e);
    nlohmann::json result = {{"status", "deleted"}};
    logWebAction(server, "patches.delete", "ok", {{"id", id}});
    return result;
  });

  server.router().on("patches.move", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id") || !params.contains("x") || !params.contains("y")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameters: id, x, y"};
    }
    int id = params["id"].get<int>();
    int new_x = params["x"].get<int>();
    int new_y = params["y"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto e = static_cast<entt::entity>(id);

    if (!state.registry.valid(e) || !state.registry.all_of<ResourcePatch>(e)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Patch not found"};
    }

    auto& patch = state.registry.get<ResourcePatch>(e);
    int dx = new_x - patch.min_x;
    int dy = new_y - patch.min_y;
    for (auto& [cx, cy] : patch.cells) {
      cx += dx;
      cy += dy;
    }
    patch.recalculateBounds();
    state.registry.replace<ResourcePatch>(e, patch);

    if (state.registry.all_of<wl::transform>(e)) {
      auto& t = state.registry.get<wl::transform>(e);
      t.position.x = static_cast<float>(patch.min_x * 25);
      t.position.y = static_cast<float>(patch.min_y * 25);
      state.registry.replace<wl::transform>(e, t);
    }

    logWebAction(server, "patches.move", "ok", {{"id", id}, {"x", new_x}, {"y", new_y}});
    return {{"ok", true}};
  });

  server.router().on("patches.get", [](const Context& /*ctx*/, const nlohmann::json& params) -> nlohmann::json {
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    if (!params.contains("id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required parameter: id"};
    }
    int id = params["id"].get<int>();
    
    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto e = static_cast<entt::entity>(id);
    
    if (!state.registry.valid(e) || !state.registry.all_of<ResourcePatch>(e)) {
      throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Patch not found"};
    }
    
    auto& patch = state.registry.get<ResourcePatch>(e);
    auto& meta = state.registry.get<hf::meta>(e);
    return serializePatch(e, patch, meta);
  });
}

} // namespace rpc
