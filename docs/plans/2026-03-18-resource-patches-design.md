# Resource Patches Design

## Summary

Resource patches are grid-based blobs of mineable resources (e.g., sparkstone ore) rendered below frames on the canvas. Miners must overlap a patch to extract resources at their configured rate. Patches are entt entities with components, support both procedural generation and manual placement, and have Lua-defined patch types.

## Requirements

- **Infinite resources**: Rate-limited by miner speed, not depletion
- **Organic blob shapes**: Cellular automata generation (hellfrost-style)
- **Placement**: Both procedural generation and manual editing
- **Miner interaction**: Frame must overlap/touch the patch to extract
- **Storage**: List of cell coordinates (Option B from analysis)
- **Extensible**: Single item type now (sparkstone), extensible to multiple types later

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Lua Definitions (scripts/patches/*.lua)                   │
│  - Patch types with item reference, color, generation params│
└─────────────────────┬───────────────────────────────────────┘
                      │ loaded at startup
                      ▼
┌─────────────────────────────────────────────────────────────┐
│  Backend (C++)                                              │
│  - ResourcePatch component (cells, item_name, bounds)       │
│  - PatchLoader (loads patch type definitions)               │
│  - Blob generation (adapted from hellfrost makeBlob)        │
│  - RPC handlers (patches.list, patches.create, etc.)        │
└─────────────────────┬───────────────────────────────────────┘
                      │ WebSocket RPC
                      ▼
┌─────────────────────────────────────────────────────────────┐
│  Web Frontend (TypeScript/PixiJS)                           │
│  - Patch layer rendered below frame layer                   │
│  - Each cell = semi-transparent colored rect                │
│  - Mini-inspector, state editor, context menus              │
└─────────────────────────────────────────────────────────────┘
```

## Data Model

### ECS Components (Backend)

Resource patches are entt entities with these components:

| Component | Purpose |
|-----------|---------|
| `hf::meta` | name, description, id |
| `wl::transform` | position on canvas (top-left of bounding box) |
| `ResourcePatch` | patch-specific data (cells, item, type) |
| `wl::relation` | parent = "Resource Patches" folder entity |

### ResourcePatch Component

```cpp
// include/game/components/resource_patch.hpp

struct ResourcePatch {
  std::string patch_type;  // key from scripts/patches/ (e.g. "sparkstone_deposit")
  std::string item_name;   // resolved from patch type definition
  std::vector<std::pair<int, int>> cells;  // grid coordinates
  
  // Cached bounding box (recalculated when cells change)
  int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
  
  void recalculateBounds();
  bool containsCell(int x, int y) const;
  bool overlapsRect(int rx, int ry, int rw, int rh) const;
  
  // Serialization
  template <class Archive> void save(Archive &ar) const;
  template <class Archive> void load(Archive &ar);
};
```

### Lua Patch Type Definitions

New folder: `scripts/patches/`

Example `scripts/patches/sparkstone_deposit.lua`:

```lua
return {
  name = "Sparkstone Deposit",
  description = "A natural deposit of sparkstone ore",
  item = "Spark Ore",              -- references scripts/items/spark_ore.lua
  color = { r = 255, g = 200, b = 50, a = 100 },  -- semi-transparent yellow
  generation = {
    min_width = 3,
    max_width = 8,
    min_height = 3,
    max_height = 8,
    fill_probability = 0.35,       -- initial random fill for blob
    smoothing_rounds = 5,          -- cellular automata iterations
  }
}
```

### RPC DTO (JSON)

```json
{
  "id": 1,
  "name": "Sparkstone Deposit #1",
  "type": "sparkstone_deposit",
  "item": "Spark Ore",
  "cells": [[5, 3], [5, 4], [6, 3], [6, 4], [7, 4]],
  "bounds": { "x": 5, "y": 3, "w": 3, "h": 2 },
  "color": { "r": 255, "g": 200, "b": 50, "a": 100 }
}
```

## Backend Implementation

### PatchLoader

Parallel to existing `ItemLoader`:

```cpp
// include/game/patch_loader.hpp

struct PatchTypeDefinition {
  std::string key;           // filename without .lua
  std::string name;
  std::string description;
  std::string item;          // item name reference
  struct { uint8_t r, g, b, a; } color;
  struct {
    int min_width, max_width;
    int min_height, max_height;
    float fill_probability;
    int smoothing_rounds;
  } generation;
};

class PatchLoader {
public:
  void load_patches(const std::string& path, sol::state& lua);
  const std::unordered_map<std::string, PatchTypeDefinition>& get_patch_types() const;
  const PatchTypeDefinition* get_patch_type(const std::string& key) const;
};
```

### Blob Generation

Adapted from hellfrost `Room::makeBlob()`:

```cpp
// include/game/systems/patch_generation.hpp

std::vector<std::pair<int, int>> generateBlob(
  int width, int height,
  float fill_probability,
  int smoothing_rounds
);
```

Algorithm:
1. Create width×height grid
2. Fill cells with `fill_probability` chance
3. Apply cellular automata smoothing `smoothing_rounds` times:
   - Cell with <4 filled neighbors becomes empty
   - Cell with ≥6 filled neighbors becomes filled
4. Return list of filled cell coordinates

### RPC Handlers

New handler: `include/rpc/handlers/patch_handler.hpp`

| Endpoint | Method | Description |
|----------|--------|-------------|
| `patches.list` | GET | List all resource patches |
| `patches.types` | GET | List available patch types from Lua definitions |
| `patches.create` | POST | Create patch at position (procedural generation) |
| `patches.delete` | POST | Remove a patch by ID |
| `patches.get` | GET | Get single patch details |
| `patches.move` | POST | Move patch to new position |
| `patches.update` | POST | Update patch name/metadata |

### Miner Overlap Detection

In `ItemsSystem::fixedUpdate()`:

```cpp
// For Miner components, check frame overlap with patches
auto& current_state = entt::locator<State>::value();

for (auto frame_entity : current_state.registry.view<Frame>()) {
  auto& frame = current_state.registry.get<Frame>(frame_entity);
  auto& transform = current_state.registry.get<wl::transform>(frame_entity);
  
  // Get cells occupied by this frame
  auto frame_cells = getFrameOccupiedCells(transform.position, frame.size);
  
  for (auto& miner : frame.components) {
    if (miner->data.get<std::string>("type") != "Miner") continue;
    if (miner->state != ComponentState::ACTIVE) continue;
    
    // Find overlapping patch
    for (auto patch_entity : current_state.registry.view<ResourcePatch>()) {
      auto& patch = current_state.registry.get<ResourcePatch>(patch_entity);
      if (cellsOverlap(frame_cells, patch.cells)) {
        // Miner can extract patch.item_name
        // Set miner recipe based on patch item
        break;
      }
    }
  }
}
```

## Web Frontend Implementation

### State Store

New store: `web/src/stores/patches.ts`

```typescript
interface Patch {
  id: number;
  name: string;
  type: string;
  item: string;
  cells: [number, number][];
  bounds: { x: number; y: number; w: number; h: number };
  color: { r: number; g: number; b: number; a: number };
}

interface PatchType {
  key: string;
  name: string;
  item: string;
  color: { r: number; g: number; b: number; a: number };
}

interface PatchStore {
  patches: Patch[];
  patchTypes: PatchType[];
  selectedPatchId: number | null;
  // Actions
  fetchPatches: () => Promise<void>;
  fetchPatchTypes: () => Promise<void>;
  createPatch: (type: string, x: number, y: number) => Promise<void>;
  deletePatch: (id: number) => Promise<void>;
  selectPatch: (id: number | null) => void;
}
```

### Canvas Rendering

In `GameCanvas.tsx`:

```typescript
// Add patch layer BEFORE frame layer
const patchLayerRef = useRef<Container | null>(null);

// In setup:
const patchLayer = new Container();
patchLayer.label = "patches";
world.addChild(patchLayer);  // Added before frameLayer
patchLayerRef.current = patchLayer;

// Render patches:
function renderPatches(patches: Patch[]) {
  const layer = patchLayerRef.current;
  if (!layer) return;
  layer.removeChildren();
  
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
}
```

### Patch Mini-Inspector

New component: `web/src/components/canvas/PatchMiniInspector.tsx`

Displays when clicking a patch on canvas:
- Patch name (editable inline)
- Patch type
- Associated item (with icon)
- Cell count
- Bounding box dimensions
- Delete button

### State Editor Integration

In sidebar/inspector, add "Resource Patches" section:
- Collapsible tree listing all patches
- Each patch entry shows:
  - Name (editable)
  - Type (dropdown)
  - Item (read-only)
  - Position (editable)
  - Size info

### Context Menu Integration

**Canvas right-click (empty space):**
- "Create Resource Patch" → submenu with patch types
- Creates patch at clicked grid position

**Patch right-click:**
- "Inspect Patch"
- "Delete Patch"

## File Changes Summary

### New Files

| Path | Description |
|------|-------------|
| `include/game/components/resource_patch.hpp` | ResourcePatch component |
| `include/game/patch_loader.hpp` | PatchLoader class |
| `include/game/systems/patch_generation.hpp` | Blob generation algorithm |
| `include/rpc/handlers/patch_handler.hpp` | RPC handler declarations |
| `src/game/patch_loader.cpp` | PatchLoader implementation |
| `src/game/systems/patch_generation.cpp` | Blob generation implementation |
| `src/rpc/handlers/patch_handler.cpp` | RPC handler implementation |
| `scripts/patches/sparkstone_deposit.lua` | Initial patch type definition |
| `web/src/stores/patches.ts` | Patch state store |
| `web/src/components/canvas/PatchMiniInspector.tsx` | Mini-inspector component |
| `web/src/components/panels/PatchInspectorPanel.tsx` | State editor panel |

### Modified Files

| Path | Changes |
|------|---------|
| `src/game/game_manager.cpp` | Load patch types, add PatchLoader |
| `src/game/systems/items.cpp` | Miner-patch overlap detection |
| `src/rpc/server.cpp` | Register patch handlers |
| `web/src/components/canvas/GameCanvas.tsx` | Patch layer rendering |
| `web/src/components/canvas/ComponentContextMenu.tsx` | Add patch creation option |
| `web/src/App.tsx` | Include patch store initialization |

## Reused from Hellfrost

The blob generation algorithm is adapted from:
- `hellfrost/src/game/room.cpp` - `Room::makeBlob()` and `Room::_makeBlob()`

Key adaptations:
- Remove Room/Cell class dependencies
- Return flat coordinate list instead of Cell grid
- Simplify to just the cellular automata core logic

## Future Extensions

- Multiple patch types with different items
- Patch depletion (finite resources)
- Patch discovery/fog of war
- Patch richness/yield modifiers
- Visual variants (different blob textures)
