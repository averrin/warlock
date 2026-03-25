#include <rpc/handlers/patch_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <game/components/resource_patch.hpp>
#include <game/patch_loader.hpp>
#include <game/systems/patch_generation.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/well_known_entities.hpp>
#include <utils/entt.hpp>
#include <utils/entt_draw.hpp>
#include <utils/entt_tools.hpp>
#include <algorithm>
#include <fmt/core.h>
#include <random>
#include <vector>

namespace rpc {

namespace {

void reparentOrphanPatches(entt::registry& reg, entt::entity folder) {
  if (folder == entt::null || !reg.valid(folder)) return;
  auto& folder_rel = reg.get_or_emplace<wl::relation>(folder);
  for (auto e : reg.view<ResourcePatch>()) {
    auto& rel = reg.get_or_emplace<wl::relation>(e);
    if (rel.parent != entt::null) continue;
    rel.parent = folder;
    if (std::find(folder_rel.children.begin(), folder_rel.children.end(), e) == folder_rel.children.end()) {
      folder_rel.children.push_back(e);
    }
  }
}

entt::entity ensurePatchesFolder(entt::registry& reg) {
  auto& wk = entt::locator<WellKnownEntities>::value();
  if (wk.patches_folder != entt::null && reg.valid(wk.patches_folder)) {
    return wk.patches_folder;
  }
  for (auto e : reg.view<hf::meta>()) {
    if (reg.get<hf::meta>(e).name == "Patches") {
      wk.patches_folder = e;
      return e;
    }
  }
  wk.patches_folder = EnttTools::createEntityFromPrototype("FOLDER", reg, "Patches");
  reparentOrphanPatches(reg, wk.patches_folder);
  return wk.patches_folder;
}

} // namespace

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
        {"paint_surface", def.paint_surface},
        {"z_index", def.z_index},
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

  /** Remove grid cells from paint_surface patches inside [x,x+w) × [y,y+h) (grid coordinates, same as patches.create). */
  server.router().on("patches.remove_surface_rect", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }
    int x = params.value("x", 0);
    int y = params.value("y", 0);
    int w = params.value("width", 1);
    int h = params.value("height", 1);
    w = std::clamp(w, 1, 256);
    h = std::clamp(h, 1, 256);

    auto& loader = entt::locator<PatchLoader>::value();
    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();

    std::vector<entt::entity> to_destroy;
    for (auto e : state.registry.view<ResourcePatch>()) {
      auto& patch = state.registry.get<ResourcePatch>(e);
      auto* def = loader.get_patch_type(patch.patch_type);
      if (!def || !def->paint_surface) {
        continue;
      }
      auto& cells = patch.cells;
      const auto it = std::remove_if(cells.begin(), cells.end(), [&](const std::pair<int, int>& p) {
        const int cx = p.first;
        const int cy = p.second;
        return cx >= x && cx < x + w && cy >= y && cy < y + h;
      });
      if (it == cells.end()) {
        continue;
      }
      cells.erase(it, cells.end());
      if (cells.empty()) {
        to_destroy.push_back(e);
        continue;
      }
      patch.recalculateBounds();
      state.registry.replace<ResourcePatch>(e, patch);
      if (state.registry.all_of<wl::transform>(e)) {
        auto& t = state.registry.get<wl::transform>(e);
        t.position.x = static_cast<float>(patch.min_x * 25);
        t.position.y = static_cast<float>(patch.min_y * 25);
        state.registry.replace<wl::transform>(e, t);
      }
    }
    for (auto e : to_destroy) {
      state.registry.destroy(e);
    }
    logWebAction(server, "patches.remove_surface_rect", "ok", {{"x", x}, {"y", y}, {"width", w}, {"height", h}});
    return {{"ok", true}, {"removed_entities", static_cast<int>(to_destroy.size())}};
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

    std::vector<std::pair<int, int>> cells;
    if (def->paint_surface) {
      int pw = params.value("width", 1);
      int ph = params.value("height", 1);
      pw = std::clamp(pw, 1, 256);
      ph = std::clamp(ph, 1, 256);
      cells.reserve(static_cast<size_t>(pw) * static_cast<size_t>(ph));
      for (int cy = 0; cy < ph; ++cy) {
        for (int cx = 0; cx < pw; ++cx) {
          cells.emplace_back(x + cx, y + cy);
        }
      }
    } else {
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
    
    cells = generateBlob(width, height, def->generation.fill_probability, 
                              def->generation.smoothing_rounds);
    
    for (auto& [cx, cy] : cells) {
      cx += x;
      cy += y;
    }
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

    entt::entity folder = ensurePatchesFolder(state.registry);
    auto& rel = state.registry.get_or_emplace<wl::relation>(e);
    rel.parent = folder;
    auto& prel = state.registry.get_or_emplace<wl::relation>(folder);
    if (std::find(prel.children.begin(), prel.children.end(), e) == prel.children.end()) {
      prel.children.push_back(e);
    }
    
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
    if (state.registry.all_of<wl::relation>(e)) {
      auto& rel = state.registry.get<wl::relation>(e);
      if (rel.parent != entt::null && state.registry.valid(rel.parent) &&
          state.registry.all_of<wl::relation>(rel.parent)) {
        auto& pch = state.registry.get<wl::relation>(rel.parent).children;
        pch.erase(std::remove(pch.begin(), pch.end(), e), pch.end());
      }
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
