#include <rpc/handlers/mapgen_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <game/systems/map_generator.hpp>
#include <game/mapgen_loader.hpp>
#include <game/components/resource_patch.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/well_known_entities.hpp>
#include <utils/entt.hpp>
#include <utils/entt_draw.hpp>
#include <utils/entt_tools.hpp>
#include <fmt/core.h>
#include <filesystem>

namespace rpc {

void registerMapgenHandlers(Server& server) {

  // ── map.generate ──────────────────────────────────────────────────────────
  server.router().on("map.generate", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) {
      throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    }

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& lua = entt::locator<sol::state>::value();
    namespace fs = std::filesystem;
    fs::path PATH = entt::monostate<"path"_hs>{};

    // Load mapgen config
    MapGenLoader loader;
    loader.load((PATH / "scripts" / "mapgen").string(), lua);

    auto config = loader.config();

    // Override seed from params
    if (params.contains("seed") && params["seed"].is_number()) {
      config.seed = params["seed"].get<uint64_t>();
    }
    // Override dimensions from params
    if (params.contains("config") && params["config"].is_object()) {
      auto& cfg = params["config"];
      if (cfg.contains("width")) config.width = cfg["width"].get<int>();
      if (cfg.contains("height")) config.height = cfg["height"].get<int>();
    }

    // Clear existing patches
    std::vector<entt::entity> to_destroy;
    for (auto e : state.registry.view<ResourcePatch>()) {
      to_destroy.push_back(e);
    }
    // Unlink from patches folder
    auto& wk = entt::locator<WellKnownEntities>::value();
    if (wk.patches_folder != entt::null && state.registry.valid(wk.patches_folder) &&
        state.registry.all_of<wl::relation>(wk.patches_folder)) {
      auto& pch = state.registry.get<wl::relation>(wk.patches_folder).children;
      pch.erase(std::remove_if(pch.begin(), pch.end(),
                  [&to_destroy](entt::entity e) {
                    return std::find(to_destroy.begin(), to_destroy.end(), e) != to_destroy.end();
                  }),
                pch.end());
    }
    for (auto e : to_destroy) {
      state.registry.destroy(e);
    }

    // Ensure patches folder exists
    if (wk.patches_folder == entt::null || !state.registry.valid(wk.patches_folder)) {
      bool found = false;
      for (auto e : state.registry.view<hf::meta>()) {
        if (state.registry.get<hf::meta>(e).name == "Patches") {
          wk.patches_folder = e;
          found = true;
          break;
        }
      }
      if (!found) {
        wk.patches_folder = EnttTools::createEntityFromPrototype("FOLDER", state.registry, "Patches");
      }
    }

    // Generate
    MapGenerator gen;
    auto result = gen.generate(config, loader.biomes(), loader.features(),
                               state.registry, lua);

    logWebAction(server, "map.generate", "ok",
                 {{"seed", result.seed_used}, {"patches", result.patches_placed}});

    return {
      {"seed_used", result.seed_used},
      {"patches_placed", result.patches_placed},
      {"summary", result.summary},
      {"biomes", result.biomes_used}
    };
  });

  // ── map.modify_cells ──────────────────────────────────────────────────────
  server.router().on("map.modify_cells", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
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

    auto& patch = state.registry.get<ResourcePatch>(e);

    // Remove cells
    if (params.contains("remove") && params["remove"].is_array()) {
      for (const auto& cell : params["remove"]) {
        if (!cell.is_array() || cell.size() < 2) continue;
        int rx = cell[0].get<int>();
        int ry = cell[1].get<int>();
        auto it = std::remove(patch.cells.begin(), patch.cells.end(), std::make_pair(rx, ry));
        patch.cells.erase(it, patch.cells.end());
      }
    }

    // Add cells
    if (params.contains("add") && params["add"].is_array()) {
      for (const auto& cell : params["add"]) {
        if (!cell.is_array() || cell.size() < 2) continue;
        int ax = cell[0].get<int>();
        int ay = cell[1].get<int>();
        auto p = std::make_pair(ax, ay);
        if (std::find(patch.cells.begin(), patch.cells.end(), p) == patch.cells.end()) {
          patch.cells.push_back(p);
        }
      }
    }

    // Handle empty patch
    if (patch.cells.empty()) {
      // Destroy the entity
      if (state.registry.all_of<wl::relation>(e)) {
        auto& rel = state.registry.get<wl::relation>(e);
        if (rel.parent != entt::null && state.registry.valid(rel.parent) &&
            state.registry.all_of<wl::relation>(rel.parent)) {
          auto& pch = state.registry.get<wl::relation>(rel.parent).children;
          pch.erase(std::remove(pch.begin(), pch.end(), e), pch.end());
        }
      }
      state.registry.destroy(e);
      logWebAction(server, "map.modify_cells", "destroyed", {{"id", id}});
      return {{"ok", true}, {"cell_count", 0}, {"destroyed", true}};
    }

    patch.recalculateBounds();
    state.registry.replace<ResourcePatch>(e, patch);

    if (state.registry.all_of<wl::transform>(e)) {
      auto& t = state.registry.get<wl::transform>(e);
      t.position.x = static_cast<float>(patch.min_x * 25);
      t.position.y = static_cast<float>(patch.min_y * 25);
      state.registry.replace<wl::transform>(e, t);
    }

    logWebAction(server, "map.modify_cells", "ok",
                 {{"id", id}, {"cell_count", static_cast<int>(patch.cells.size())}});
    return {{"ok", true}, {"cell_count", static_cast<int>(patch.cells.size())}};
  });
}

} // namespace rpc
