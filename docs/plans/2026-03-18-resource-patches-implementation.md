# Resource Patches Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add grid-based resource patches that miners can extract from, rendered as semi-transparent blobs below frames on the canvas.

**Architecture:** Resource patches are entt entities with a `ResourcePatch` component storing cell coordinates. Patch types are defined in Lua (`scripts/patches/`). The web frontend renders patches in a dedicated PixiJS layer below frames. Miners check for overlap with patches to determine what item they can extract.

**Tech Stack:** C++/entt (backend), Lua (patch definitions), TypeScript/PixiJS (web frontend), WebSocket RPC (communication)

---

## Task 1: ResourcePatch Component

**Files:**
- Create: `include/game/components/resource_patch.hpp`

**Step 1: Create the ResourcePatch component header**

```cpp
#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

struct ResourcePatch {
  std::string patch_type;
  std::string item_name;
  std::vector<std::pair<int, int>> cells;
  
  int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
  
  void recalculateBounds() {
    if (cells.empty()) {
      min_x = min_y = max_x = max_y = 0;
      return;
    }
    min_x = max_x = cells[0].first;
    min_y = max_y = cells[0].second;
    for (const auto& [x, y] : cells) {
      if (x < min_x) min_x = x;
      if (x > max_x) max_x = x;
      if (y < min_y) min_y = y;
      if (y > max_y) max_y = y;
    }
  }
  
  bool containsCell(int x, int y) const {
    return std::find(cells.begin(), cells.end(), std::make_pair(x, y)) != cells.end();
  }
  
  bool overlapsRect(int rx, int ry, int rw, int rh) const {
    for (const auto& [cx, cy] : cells) {
      if (cx >= rx && cx < rx + rw && cy >= ry && cy < ry + rh) {
        return true;
      }
    }
    return false;
  }
  
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(patch_type, item_name, cells, min_x, min_y, max_x, max_y);
  }
  template <class Archive> void load(Archive &ar) {
    ar(patch_type, item_name, cells, min_x, min_y, max_x, max_y);
  }
};
```

**Step 2: Commit**

```bash
git add include/game/components/resource_patch.hpp
git commit -m "feat: add ResourcePatch component"
```

---

## Task 2: Patch Type Definitions (Lua)

**Files:**
- Create: `scripts/patches/sparkstone_deposit.lua`

**Step 1: Create patches directory and first patch type**

```lua
return {
  name = "Sparkstone Deposit",
  description = "A natural deposit of sparkstone ore",
  item = "Spark Ore",
  color = { r = 255, g = 200, b = 50, a = 100 },
  generation = {
    min_width = 3,
    max_width = 8,
    min_height = 3,
    max_height = 8,
    fill_probability = 0.35,
    smoothing_rounds = 5,
  }
}
```

**Step 2: Commit**

```bash
git add scripts/patches/sparkstone_deposit.lua
git commit -m "feat: add sparkstone deposit patch type definition"
```

---

## Task 3: PatchLoader

**Files:**
- Create: `include/game/patch_loader.hpp`
- Create: `src/game/patch_loader.cpp`

**Step 1: Create PatchLoader header**

```cpp
#pragma once

#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

struct PatchColor {
  uint8_t r = 255, g = 200, b = 50, a = 100;
};

struct PatchGeneration {
  int min_width = 3, max_width = 8;
  int min_height = 3, max_height = 8;
  float fill_probability = 0.35f;
  int smoothing_rounds = 5;
};

struct PatchTypeDefinition {
  std::string key;
  std::string name;
  std::string description;
  std::string item;
  PatchColor color;
  PatchGeneration generation;
};

class PatchLoader {
public:
  void load_patches(const std::string& path, sol::state& lua);
  const std::unordered_map<std::string, PatchTypeDefinition>& get_patch_types() const;
  const PatchTypeDefinition* get_patch_type(const std::string& key) const;
  
private:
  std::unordered_map<std::string, PatchTypeDefinition> patch_types_;
};
```

**Step 2: Create PatchLoader implementation**

```cpp
#include <game/patch_loader.hpp>
#include <filesystem>
#include <fmt/core.h>

namespace fs = std::filesystem;

void PatchLoader::load_patches(const std::string& path, sol::state& lua) {
  patch_types_.clear();
  
  if (!fs::exists(path)) {
    return;
  }
  
  for (const auto& entry : fs::directory_iterator(path)) {
    if (entry.path().extension() != ".lua") continue;
    
    std::string key = entry.path().stem().string();
    sol::table t = lua.load_file(entry.path().string()).call();
    
    PatchTypeDefinition def;
    def.key = key;
    def.name = t.get_or<std::string>("name", key);
    def.description = t.get_or<std::string>("description", "");
    def.item = t.get_or<std::string>("item", "");
    
    if (sol::table color = t["color"]; color.valid()) {
      def.color.r = color.get_or<uint8_t>("r", 255);
      def.color.g = color.get_or<uint8_t>("g", 200);
      def.color.b = color.get_or<uint8_t>("b", 50);
      def.color.a = color.get_or<uint8_t>("a", 100);
    }
    
    if (sol::table gen = t["generation"]; gen.valid()) {
      def.generation.min_width = gen.get_or<int>("min_width", 3);
      def.generation.max_width = gen.get_or<int>("max_width", 8);
      def.generation.min_height = gen.get_or<int>("min_height", 3);
      def.generation.max_height = gen.get_or<int>("max_height", 8);
      def.generation.fill_probability = gen.get_or<float>("fill_probability", 0.35f);
      def.generation.smoothing_rounds = gen.get_or<int>("smoothing_rounds", 5);
    }
    
    patch_types_[key] = def;
  }
}

const std::unordered_map<std::string, PatchTypeDefinition>& 
PatchLoader::get_patch_types() const {
  return patch_types_;
}

const PatchTypeDefinition* PatchLoader::get_patch_type(const std::string& key) const {
  auto it = patch_types_.find(key);
  return it != patch_types_.end() ? &it->second : nullptr;
}
```

**Step 3: Add to CMakeLists.txt**

Modify `src/CMakeLists.txt` to include `game/patch_loader.cpp` in the source list.

**Step 4: Commit**

```bash
git add include/game/patch_loader.hpp src/game/patch_loader.cpp
git commit -m "feat: add PatchLoader for loading patch type definitions"
```

---

## Task 4: Blob Generation Algorithm

**Files:**
- Create: `include/game/systems/patch_generation.hpp`
- Create: `src/game/systems/patch_generation.cpp`

**Step 1: Create blob generation header**

```cpp
#pragma once

#include <utility>
#include <vector>

std::vector<std::pair<int, int>> generateBlob(
  int width, int height,
  float fill_probability,
  int smoothing_rounds
);
```

**Step 2: Create blob generation implementation**

Adapted from hellfrost `Room::makeBlob()`:

```cpp
#include <game/systems/patch_generation.hpp>
#include <random>
#include <algorithm>

std::vector<std::pair<int, int>> generateBlob(
  int width, int height,
  float fill_probability,
  int smoothing_rounds
) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dis(0.0f, 1.0f);
  
  std::vector<std::vector<bool>> grid(height, std::vector<bool>(width, false));
  
  // Initial random fill
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      grid[y][x] = dis(gen) < fill_probability;
    }
  }
  
  // Cellular automata smoothing
  for (int round = 0; round < smoothing_rounds; ++round) {
    std::vector<std::vector<bool>> next = grid;
    
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        int neighbors = 0;
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
              if (grid[ny][nx]) neighbors++;
            }
          }
        }
        
        if (grid[y][x]) {
          next[y][x] = neighbors >= 4;
        } else {
          next[y][x] = neighbors >= 6;
        }
      }
    }
    
    grid = next;
  }
  
  // Convert to coordinate list
  std::vector<std::pair<int, int>> result;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (grid[y][x]) {
        result.emplace_back(x, y);
      }
    }
  }
  
  return result;
}
```

**Step 3: Commit**

```bash
git add include/game/systems/patch_generation.hpp src/game/systems/patch_generation.cpp
git commit -m "feat: add blob generation algorithm for resource patches"
```

---

## Task 5: RPC Patch Handler

**Files:**
- Create: `include/rpc/handlers/patch_handler.hpp`
- Create: `src/rpc/handlers/patch_handler.cpp`
- Modify: `src/rpc/server.cpp`

**Step 1: Create patch handler header**

```cpp
#pragma once

#include <nlohmann/json.hpp>

namespace rpc {

nlohmann::json handle_patches_list(const nlohmann::json& params);
nlohmann::json handle_patches_types(const nlohmann::json& params);
nlohmann::json handle_patches_create(const nlohmann::json& params);
nlohmann::json handle_patches_delete(const nlohmann::json& params);
nlohmann::json handle_patches_get(const nlohmann::json& params);

} // namespace rpc
```

**Step 2: Create patch handler implementation**

```cpp
#include <rpc/handlers/patch_handler.hpp>
#include <game/components/resource_patch.hpp>
#include <game/patch_loader.hpp>
#include <game/systems/patch_generation.hpp>
#include <game/state.hpp>
#include <utils/entt.hpp>
#include <utils/entt_draw.hpp>
#include <entt/entt.hpp>

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
    {"cells", cells_arr},
    {"bounds", {{"x", patch.min_x}, {"y", patch.min_y}, 
                {"w", patch.max_x - patch.min_x + 1}, 
                {"h", patch.max_y - patch.min_y + 1}}},
    {"color", color}
  };
}

nlohmann::json handle_patches_list(const nlohmann::json&) {
  auto& state = entt::locator<State>::value();
  nlohmann::json patches = nlohmann::json::array();
  
  for (auto e : state.registry.view<ResourcePatch>()) {
    auto& patch = state.registry.get<ResourcePatch>(e);
    auto& meta = state.registry.get<hf::meta>(e);
    patches.push_back(serializePatch(e, patch, meta));
  }
  
  return {{"patches", patches}};
}

nlohmann::json handle_patches_types(const nlohmann::json&) {
  auto& loader = entt::locator<PatchLoader>::value();
  nlohmann::json types = nlohmann::json::array();
  
  for (const auto& [key, def] : loader.get_patch_types()) {
    types.push_back({
      {"key", key},
      {"name", def.name},
      {"item", def.item},
      {"color", {{"r", def.color.r}, {"g", def.color.g}, 
                 {"b", def.color.b}, {"a", def.color.a}}}
    });
  }
  
  return {{"types", types}};
}

nlohmann::json handle_patches_create(const nlohmann::json& params) {
  std::string type = params.value("type", "sparkstone_deposit");
  int x = params.value("x", 0);
  int y = params.value("y", 0);
  
  auto& loader = entt::locator<PatchLoader>::value();
  auto* def = loader.get_patch_type(type);
  if (!def) {
    return {{"error", "Unknown patch type"}};
  }
  
  auto& state = entt::locator<State>::value();
  
  // Generate blob
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> w_dis(def->generation.min_width, def->generation.max_width);
  std::uniform_int_distribution<int> h_dis(def->generation.min_height, def->generation.max_height);
  
  int width = w_dis(gen);
  int height = h_dis(gen);
  
  auto cells = generateBlob(width, height, def->generation.fill_probability, 
                            def->generation.smoothing_rounds);
  
  // Offset cells to world position
  for (auto& [cx, cy] : cells) {
    cx += x;
    cy += y;
  }
  
  // Create entity
  auto e = state.registry.create();
  
  hf::meta meta;
  meta.name = def->name;
  meta.id = fmt::format("PATCH-{}", static_cast<int>(e));
  state.registry.emplace<hf::meta>(e, meta);
  
  wl::transform transform;
  transform.position.x = static_cast<float>(x * 75);  // CELL size
  transform.position.y = static_cast<float>(y * 75);
  state.registry.emplace<wl::transform>(e, transform);
  
  ResourcePatch patch;
  patch.patch_type = type;
  patch.item_name = def->item;
  patch.cells = cells;
  patch.recalculateBounds();
  state.registry.emplace<ResourcePatch>(e, patch);
  
  return {{"id", static_cast<int>(e)}, {"status", "created"}};
}

nlohmann::json handle_patches_delete(const nlohmann::json& params) {
  int id = params.value("id", -1);
  if (id < 0) return {{"error", "Invalid patch ID"}};
  
  auto& state = entt::locator<State>::value();
  auto e = static_cast<entt::entity>(id);
  
  if (!state.registry.valid(e) || !state.registry.all_of<ResourcePatch>(e)) {
    return {{"error", "Patch not found"}};
  }
  
  state.registry.destroy(e);
  return {{"status", "deleted"}};
}

nlohmann::json handle_patches_get(const nlohmann::json& params) {
  int id = params.value("id", -1);
  if (id < 0) return {{"error", "Invalid patch ID"}};
  
  auto& state = entt::locator<State>::value();
  auto e = static_cast<entt::entity>(id);
  
  if (!state.registry.valid(e) || !state.registry.all_of<ResourcePatch>(e)) {
    return {{"error", "Patch not found"}};
  }
  
  auto& patch = state.registry.get<ResourcePatch>(e);
  auto& meta = state.registry.get<hf::meta>(e);
  return serializePatch(e, patch, meta);
}

} // namespace rpc
```

**Step 3: Register handlers in server.cpp**

Add to the handler registration section:

```cpp
#include <rpc/handlers/patch_handler.hpp>

// In registerHandlers() or equivalent:
handlers_["patches.list"] = rpc::handle_patches_list;
handlers_["patches.types"] = rpc::handle_patches_types;
handlers_["patches.create"] = rpc::handle_patches_create;
handlers_["patches.delete"] = rpc::handle_patches_delete;
handlers_["patches.get"] = rpc::handle_patches_get;
```

**Step 4: Commit**

```bash
git add include/rpc/handlers/patch_handler.hpp src/rpc/handlers/patch_handler.cpp
git commit -m "feat: add RPC handlers for resource patches"
```

---

## Task 6: Load Patch Types at Startup

**Files:**
- Modify: `src/game/game_manager.cpp`

**Step 1: Add PatchLoader initialization**

In `GameManager::loadData()`, add after loading items:

```cpp
#include <game/patch_loader.hpp>

// In loadData():
log.start("Loading Patch Types");
auto& patch_loader = entt::locator<PatchLoader>::emplace();
fs::path patches_path = PATH / "scripts" / "patches";
patch_loader.load_patches(patches_path.string(), lua);
log.var("Patch Types", patch_loader.get_patch_types().size());
log.stop("Loading Patch Types");
```

**Step 2: Commit**

```bash
git add src/game/game_manager.cpp
git commit -m "feat: load patch type definitions at startup"
```

---

## Task 7: Web Frontend - Patches Store

**Files:**
- Create: `web/src/stores/patches.ts`

**Step 1: Create patches store**

```typescript
import { create } from "zustand";

export interface PatchCell {
  x: number;
  y: number;
}

export interface Patch {
  id: number;
  name: string;
  type: string;
  item: string;
  cells: [number, number][];
  bounds: { x: number; y: number; w: number; h: number };
  color: { r: number; g: number; b: number; a: number };
}

export interface PatchType {
  key: string;
  name: string;
  item: string;
  color: { r: number; g: number; b: number; a: number };
}

interface PatchStore {
  patches: Patch[];
  patchTypes: PatchType[];
  selectedPatchId: number | null;
  
  setPatches: (patches: Patch[]) => void;
  setPatchTypes: (types: PatchType[]) => void;
  addPatch: (patch: Patch) => void;
  removePatch: (id: number) => void;
  selectPatch: (id: number | null) => void;
}

export const usePatchStore = create<PatchStore>((set) => ({
  patches: [],
  patchTypes: [],
  selectedPatchId: null,
  
  setPatches: (patches) => set({ patches }),
  setPatchTypes: (types) => set({ patchTypes: types }),
  addPatch: (patch) => set((state) => ({ patches: [...state.patches, patch] })),
  removePatch: (id) => set((state) => ({ 
    patches: state.patches.filter((p) => p.id !== id),
    selectedPatchId: state.selectedPatchId === id ? null : state.selectedPatchId
  })),
  selectPatch: (id) => set({ selectedPatchId: id }),
}));
```

**Step 2: Commit**

```bash
git add web/src/stores/patches.ts
git commit -m "feat: add patches store for web frontend"
```

---

## Task 8: Web Frontend - Canvas Patch Rendering

**Files:**
- Modify: `web/src/components/canvas/GameCanvas.tsx`

**Step 1: Add patch layer and rendering**

Add imports and refs:

```typescript
import { usePatchStore } from "../../stores/patches";

// Add ref for patch layer
const patchLayerRef = useRef<Container | null>(null);
const patches = usePatchStore((s) => s.patches);
```

In canvas setup (after grid, before frameLayer):

```typescript
const patchLayer = new Container();
patchLayer.label = "patches";
world.addChild(patchLayer);
patchLayerRef.current = patchLayer;
```

Add render function:

```typescript
const renderPatches = useCallback(() => {
  const layer = patchLayerRef.current;
  if (!layer) return;
  layer.removeChildren();
  
  const CELL = 75;
  
  for (const patch of patches) {
    const gfx = new Graphics();
    const { r, g, b, a } = patch.color;
    const color = (r << 16) | (g << 8) | b;
    const alpha = a / 255;
    
    for (const [cx, cy] of patch.cells) {
      gfx.rect(cx * CELL, cy * CELL, CELL, CELL);
    }
    gfx.fill({ color, alpha });
    
    layer.addChild(gfx);
  }
}, [patches]);

// Call renderPatches when patches change
useEffect(() => {
  renderPatches();
}, [renderPatches]);
```

**Step 2: Commit**

```bash
git add web/src/components/canvas/GameCanvas.tsx
git commit -m "feat: render resource patches on canvas"
```

---

## Task 9: Web Frontend - Fetch Patches via RPC

**Files:**
- Modify: `web/src/hooks/useRpc.ts` or `web/src/App.tsx`

**Step 1: Add RPC calls to fetch patches**

In App.tsx or a dedicated hook:

```typescript
import { usePatchStore } from "./stores/patches";

// In useEffect after connection:
const fetchPatches = async () => {
  try {
    const data = await rpcClient.call<{ patches: Patch[] }>("patches.list");
    usePatchStore.getState().setPatches(data.patches ?? []);
  } catch (e) {
    console.error("Failed to fetch patches:", e);
  }
};

const fetchPatchTypes = async () => {
  try {
    const data = await rpcClient.call<{ types: PatchType[] }>("patches.types");
    usePatchStore.getState().setPatchTypes(data.types ?? []);
  } catch (e) {
    console.error("Failed to fetch patch types:", e);
  }
};

// Call on connection
fetchPatches();
fetchPatchTypes();
```

**Step 2: Commit**

```bash
git add web/src/App.tsx
git commit -m "feat: fetch patches and patch types via RPC"
```

---

## Task 10: Context Menu - Create Patch

**Files:**
- Modify: `web/src/components/canvas/GameCanvas.tsx`

**Step 1: Add patch creation to context menu**

In the canvas context menu section, add option to create patches:

```typescript
const createPatch = async (type: string, wx: number, wy: number) => {
  const CELL = 75;
  const gridX = Math.floor(wx / CELL);
  const gridY = Math.floor(wy / CELL);
  
  try {
    const result = await rpcClient.call<{ id: number }>("patches.create", {
      type,
      x: gridX,
      y: gridY,
    });
    // Refresh patches list
    const data = await rpcClient.call<{ patches: Patch[] }>("patches.list");
    usePatchStore.getState().setPatches(data.patches ?? []);
  } catch (e) {
    console.error("Failed to create patch:", e);
  }
};
```

Add to context menu JSX:

```tsx
{patchTypes.length > 0 && (
  <div className="submenu">
    <span>Create Resource Patch</span>
    <div className="submenu-items">
      {patchTypes.map((pt) => (
        <button key={pt.key} onClick={() => createPatch(pt.key, contextMenu.wx, contextMenu.wy)}>
          {pt.name}
        </button>
      ))}
    </div>
  </div>
)}
```

**Step 2: Commit**

```bash
git add web/src/components/canvas/GameCanvas.tsx
git commit -m "feat: add context menu option to create resource patches"
```

---

## Task 11: Patch Mini-Inspector

**Files:**
- Create: `web/src/components/canvas/PatchMiniInspector.tsx`

**Step 1: Create mini-inspector component**

```tsx
import { usePatchStore, Patch } from "../../stores/patches";
import { RpcClient } from "../../rpc/client";
import "./PatchMiniInspector.css";

interface Props {
  patch: Patch;
  x: number;
  y: number;
  rpcClient: RpcClient;
  onClose: () => void;
}

export function PatchMiniInspector({ patch, x, y, rpcClient, onClose }: Props) {
  const removePatch = usePatchStore((s) => s.removePatch);
  
  const handleDelete = async () => {
    try {
      await rpcClient.call("patches.delete", { id: patch.id });
      removePatch(patch.id);
      onClose();
    } catch (e) {
      console.error("Failed to delete patch:", e);
    }
  };
  
  return (
    <div 
      className="patch-mini-inspector"
      style={{ left: x, top: y }}
      data-context-menu
    >
      <div className="header">
        <span className="name">{patch.name}</span>
        <button className="close" onClick={onClose}>×</button>
      </div>
      <div className="content">
        <div className="row">
          <span className="label">Type:</span>
          <span>{patch.type}</span>
        </div>
        <div className="row">
          <span className="label">Item:</span>
          <span>{patch.item}</span>
        </div>
        <div className="row">
          <span className="label">Cells:</span>
          <span>{patch.cells.length}</span>
        </div>
        <div className="row">
          <span className="label">Size:</span>
          <span>{patch.bounds.w} × {patch.bounds.h}</span>
        </div>
      </div>
      <div className="actions">
        <button className="delete" onClick={handleDelete}>Delete</button>
      </div>
    </div>
  );
}
```

**Step 2: Add CSS**

```css
/* web/src/components/canvas/PatchMiniInspector.css */
.patch-mini-inspector {
  position: fixed;
  background: #1f2937;
  border: 1px solid #374151;
  border-radius: 8px;
  padding: 12px;
  min-width: 200px;
  z-index: 1000;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
}

.patch-mini-inspector .header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 8px;
  padding-bottom: 8px;
  border-bottom: 1px solid #374151;
}

.patch-mini-inspector .name {
  font-weight: 600;
  color: #fbbf24;
}

.patch-mini-inspector .close {
  background: none;
  border: none;
  color: #9ca3af;
  cursor: pointer;
  font-size: 18px;
}

.patch-mini-inspector .row {
  display: flex;
  justify-content: space-between;
  margin: 4px 0;
  color: #d1d5db;
}

.patch-mini-inspector .label {
  color: #9ca3af;
}

.patch-mini-inspector .actions {
  margin-top: 12px;
  padding-top: 8px;
  border-top: 1px solid #374151;
}

.patch-mini-inspector .delete {
  width: 100%;
  padding: 6px;
  background: #dc2626;
  border: none;
  border-radius: 4px;
  color: white;
  cursor: pointer;
}

.patch-mini-inspector .delete:hover {
  background: #b91c1c;
}
```

**Step 3: Commit**

```bash
git add web/src/components/canvas/PatchMiniInspector.tsx web/src/components/canvas/PatchMiniInspector.css
git commit -m "feat: add patch mini-inspector component"
```

---

## Task 12: Click Detection for Patches

**Files:**
- Modify: `web/src/components/canvas/GameCanvas.tsx`

**Step 1: Add patch click detection**

Add state for selected patch inspector:

```typescript
const [patchInspector, setPatchInspector] = useState<{ patch: Patch; x: number; y: number } | null>(null);
```

Add function to detect patch at world coordinates:

```typescript
const getPatchAtWorldPos = (wx: number, wy: number): Patch | null => {
  const CELL = 75;
  const gridX = Math.floor(wx / CELL);
  const gridY = Math.floor(wy / CELL);
  
  for (const patch of patches) {
    if (patch.cells.some(([cx, cy]) => cx === gridX && cy === gridY)) {
      return patch;
    }
  }
  return null;
};
```

In click handler, check for patch clicks:

```typescript
// If no frame clicked, check for patch
const clickedPatch = getPatchAtWorldPos(worldX, worldY);
if (clickedPatch) {
  const { x: clientX, y: clientY } = getEventClientXY(e);
  setPatchInspector({ patch: clickedPatch, x: clientX, y: clientY });
  return;
}
```

Render mini-inspector:

```tsx
{patchInspector && (
  <PatchMiniInspector
    patch={patchInspector.patch}
    x={patchInspector.x}
    y={patchInspector.y}
    rpcClient={rpcClient}
    onClose={() => setPatchInspector(null)}
  />
)}
```

**Step 2: Commit**

```bash
git add web/src/components/canvas/GameCanvas.tsx
git commit -m "feat: add click detection for patches and show mini-inspector"
```

---

## Task 13: Miner-Patch Overlap Detection

**Files:**
- Modify: `src/game/systems/items.cpp`

**Step 1: Add overlap detection in ItemsSystem**

Add helper to get frame occupied cells:

```cpp
#include <game/components/resource_patch.hpp>

namespace {
std::vector<std::pair<int, int>> getFrameOccupiedCells(const wl::transform& t, FrameSize size) {
  const int CELL = 75;
  int gridX = static_cast<int>(t.position.x) / CELL;
  int gridY = static_cast<int>(t.position.y) / CELL;
  int cells = 1;
  switch (size) {
    case FrameSize::S: cells = 1; break;
    case FrameSize::M: cells = 2; break;
    case FrameSize::L: cells = 3; break;
    case FrameSize::G: cells = 4; break;
  }
  
  std::vector<std::pair<int, int>> result;
  for (int dy = 0; dy < cells; ++dy) {
    for (int dx = 0; dx < cells; ++dx) {
      result.emplace_back(gridX + dx, gridY + dy);
    }
  }
  return result;
}

bool cellsOverlap(const std::vector<std::pair<int, int>>& a, 
                  const std::vector<std::pair<int, int>>& b) {
  for (const auto& ca : a) {
    for (const auto& cb : b) {
      if (ca.first == cb.first && ca.second == cb.second) return true;
    }
  }
  return false;
}
} // namespace
```

In `fixedUpdate()`, for Miner components check patch overlap:

```cpp
// Check if miner overlaps any resource patch
for (auto patch_e : current_state.registry.view<ResourcePatch>()) {
  auto& patch = current_state.registry.get<ResourcePatch>(patch_e);
  auto& frame_transform = current_state.registry.get<wl::transform>(f);
  auto frame_cells = getFrameOccupiedCells(frame_transform, frame.size);
  
  if (cellsOverlap(frame_cells, patch.cells)) {
    // Miner is over this patch - can extract patch.item_name
    // Could auto-set recipe or enable extraction
    break;
  }
}
```

**Step 2: Commit**

```bash
git add src/game/systems/items.cpp
git commit -m "feat: add miner-patch overlap detection"
```

---

## Task 14: Final Integration Test

**Step 1: Build and test**

```bash
just build
just run --ui-mode web
```

**Step 2: Manual test checklist**

- [ ] Open web UI
- [ ] Right-click on canvas → "Create Resource Patch" → "Sparkstone Deposit"
- [ ] Yellow semi-transparent blob appears
- [ ] Click on patch → mini-inspector opens
- [ ] Delete patch via mini-inspector
- [ ] Place miner frame overlapping a patch
- [ ] Verify miner can extract from patch

**Step 3: Commit any final fixes**

```bash
git add .
git commit -m "feat: complete resource patches feature"
```

---

## Summary

This plan implements resource patches in 14 tasks:

1. ResourcePatch component
2. Lua patch type definitions
3. PatchLoader
4. Blob generation algorithm
5. RPC handlers
6. Load patch types at startup
7. Web patches store
8. Canvas patch rendering
9. RPC fetch integration
10. Context menu creation
11. Patch mini-inspector
12. Click detection
13. Miner overlap detection
14. Integration testing
