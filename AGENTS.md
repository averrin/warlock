# Warlock (Hellfrost)

Sci-fi automation hardcore game — coding and management. Headless C++20 engine with React/TypeScript web client, connected via JSON-RPC 2.0 over WebSocket.

## Quick Start

```bash
just init          # CMake configure + symlink assets
just run           # Build & run engine (port 9800)
cd web && npm run dev  # Vite dev server (port 5173)
just test          # All tests (unit + serial + e2e)
just test-e2e      # E2E only (most important)
just restart       # Kill running engine + restart in background
just dev           # Kill → cmake build → restart (avoids LNK1168 lock)
```

## Architecture Overview

```
[React + Pixi.js web client]
        ↕ JSON-RPC 2.0 (WebSocket :9800)
[C++20 headless engine]
        ↕ Sol2 bindings
[Lua component/blueprint scripts]
```

**ECS**: EnTT registry. Fixed-timestep systems (50ms). Entities are Frames (robots), Connections (links), Environment (global state), and singleton entities (e.g. economy).

**Data flow**: Web UI → RPC call → Handler → EnTT registry mutation → Systems tick → Server broadcasts state → Zustand stores → React re-renders.

## Project Structure

```
include/game/components/frame.hpp  — Frame, Component, Connection, Metadata, Environment structs
include/game/attributes.hpp        — Attribute, Modifier, AttributeValue (variant<int,float,string,bool>)
include/game/component_registry.hpp — COMPONENT_LIST macro (single source of truth for serialization)
include/game/game_manager.hpp      — Systems, registry, spendable pool, autosave
include/game/frame_world.hpp       — FrameWorld Lua API (grid operations, movement)
include/game/system.hpp            — System base class (fixed timestep accumulator)
include/game/systems/               — Power, Thermal, CodeExecution, Items, Environment, Wireless, Tweening
include/rpc/                        — Server, Router, DTOs, handlers/
include/utils/data/field_archive.hpp — Forward-compatible named-field binary serialization
src/                                — C++ implementations (mirrors include/)
scripts/components/                 — Lua component definitions (charger.lua, miner.lua, etc.)
scripts/blueprints/                 — Lua frame templates (miner.lua, nexus.lua, etc.)
web/src/components/                 — React components (canvas/, frame/, panels/, ui/)
web/src/stores/                     — Zustand stores (game, connection, scene, patches, etc.)
web/src/rpc/                        — RPC client + TypeScript DTO types
tests/e2e/                          — E2E tests (headless engine + RPC, most important)
tests/unit/                         — Unit tests
data/                               — Binary proto/meta files (DO NOT MODIFY)
save/                               — Game save files (DO NOT MODIFY)
```

## Critical Rules

### Never Do
- Modify files in `data/` or `save/` — these are binary serialized game data
- Add code comments or docstrings unless the logic is truly non-obvious
- Create duplicate components/systems — always search for existing ones first
- Report a feature as done without verifying it actually works end-to-end
- Rush to implementation — ask clarifying questions first
- Make ad-hoc solutions — build flexible, reusable tools
- Skip running `just test-e2e` after changes to engine code

### Always Do
- Search the codebase before creating anything new
- Run `just test-e2e` after C++ changes (unit tests are less useful)
- Run `npm test` in `web/` after frontend changes
- Keep `field_save()` and `field_load()` in perfect sync (same fields, same order)
- Register new serializable components in `COMPONENT_LIST` macro
- Use `std::lock_guard<std::recursive_mutex> lock(gm.updateMutex)` when accessing registry from RPC handlers
- Get icons from https://game-icons.net/ (white, no background) for new components

## Code Style

- **C++**: Modern C++20. `auto` encouraged, syntax sugar welcome. No exceptions (use `std::expected` or similar). PascalCase for types/classes, snake_case for variables/functions. No code comments unless truly necessary. Cross-platform (Windows + Linux).
- **TypeScript**: Functional React components with hooks. Zustand for state. Inline CSSProperties (no framework). Dark theme colors hardcoded. No UI library — all custom components.
- **Lua**: Component specs return tables. snake_case. Minimal — define data and API functions only.

## Serialization System (FOOTGUN ZONE)

The save/load system uses **FieldArchive** — a named-field, forward-compatible binary format wrapping cereal.

### How It Works
- `field_save(FieldOutputArchive& ar)` / `field_load(FieldInputArchive& ar)` on each component
- `FIELD(ar, variable)` macro writes/reads named fields
- Unknown fields in save file → silently skipped (safe to remove fields)
- Missing fields in save file → C++ default value used (safe to add fields)
- No version bump needed for adding/removing fields

### Invariants
- **field_save and field_load MUST have identical fields in identical order**
- Every component in `COMPONENT_LIST` must implement both methods (empty body OK for tags)
- Never change a field's C++ type without migration logic
- Renaming a field loses old save data for that field
- After loading, `Metadata::reconcileGlobalIdCounter()` prevents ID collisions

### COMPONENT_LIST Macro
Single source of truth in `include/game/component_registry.hpp`. Adding a line here auto-enables serialization. Removing a line auto-skips old save data. No other registration needed.

### Proto Files
Files in `data/` (frame.proto, main.proto, extra.proto) are NOT Protocol Buffers — they are cereal binary archives of entity templates. Treat as opaque binary.

### RegistryStore Cereal Version (FOOTGUN)
`RegistryStore` wraps the EnTT registry with a top-level cereal binary stream for versioning. The cereal stream is **positional** — unlike FieldArchive, unknown fields cannot be skipped. If you add or remove a top-level cereal field in `RegistryStore::save()`/`load()`, you **must** bump `expected_version` and add a migration branch in `load()` for the old version, otherwise old saves load corrupted data silently. Current version: see `include/game/registry_store.hpp`.

### WellKnownEntities — Two Population Sites
`WellKnownEntities` (singleton entity handles for Environment, etc.) is populated in **both** `GameManager::loadData()` and `GameManager::start()`. Any new singleton entity must be found/created in both locations or it will be a null handle when loading from a save.

### `fs::path::stem()` on `.meta.json` Files (FOOTGUN)

Save/init state files are named `Name.meta.json`. `stem()` returns `"Name.meta"` not `"Name"` — it only strips the last extension. Always match the full suffix manually:

```cpp
auto fname = entry.path().filename().string();
if (fname.size() < 10 || fname.substr(fname.size() - 10) != ".meta.json") continue;
auto base_name = fname.substr(0, fname.size() - 10);  // "Name"
```

This applies to both `state.slots.list` and `state.init.list` handlers.

### Blueprint Code `start()` Is Never Called on Game Load (FOOTGUN)
When a game is loaded from save, Cores that were already `ACTIVE` never go through the ACTIVATING→ACTIVE transition. The `start()` function in blueprint code is published by `TweeningSystem` only when a Core's `time_switch` fires the transition — it does **not** run on load. Any Lua globals initialized in `start()` remain `nil`, causing arithmetic/nil errors on the first `update()` call and putting the Core into `COMP_ERROR`.

**Rule**: Never put essential logic inside blueprint `start()` if it needs to survive a save/load cycle. Engine-level concerns (heartbeats, registration) belong in C++ (`game_manager.cpp`), not in Lua blueprints. `recompute_data_link_counterparts()` always runs unconditionally at the top of `CodeExecutionSystem::fixedUpdate()`, so data link counterparts are always valid even when Cores are in error states.

## System Registration Order

Systems run in this order (defined in `GameManager::start()`). Order matters for data dependencies:

1. **TweeningSystem** — Attribute animation/easing
2. **PowerSystem** — Power network computation (reads frame state)
3. **ThermalSystem** — Temperature modeling (reads power data)
4. **EnvironmentSystem** — Global state (temperature, radioactivity, time)
5. **WirelessConnectionSystem** — NFC/data transmission
6. **CodeExecutionSystem** — Lua script execution (sees stable power/thermal)
7. **ItemsSystem** — Inventory/crafting (depends on component state)

## Research System

Research nodes unlock components and recipes as players progress. Scripts live in `scripts/research/`.

### Lua Format (`scripts/research/<name>.lua`)
```lua
return {
  name = "Node Name",              -- unique key used throughout the system
  description = "What it enables",
  icon = "icon-name.png",          -- from game-icons.net, white, no bg
  cost = {                         -- spendable resources consumed on unlock
    ["Electronic Parts"] = 40,
    ["Advanced Chips"] = 10,
  },
  requires = { "Prerequisite Node" },  -- names of nodes that must be unlocked first
  unlocks = { "Component Name" },      -- component types hidden until this is unlocked
  unlocks_recipes = { "Recipe Name" }, -- recipes hidden in machine UIs until unlocked
}
```

### Rules
- `cost = {}` **and** `requires = {}` → **auto-unlocked on game load** (no player action needed)
- Components absent from all `unlocks` lists → always visible/available
- Components listed in any `unlocks` → locked until the owning research node is unlocked
- `unlocks_recipes` gates recipes from appearing in machine selectors
- `requires` is checked at unlock time — prerequisites must all be unlocked first
- Costs are consumed from the spendable pool via `tryConsumeSpendable` (same pool as frame/component costs)

### Current Tree
```
Nexus Core (free) ──┬─► Basic Power ──────────────┬─► Thermal Management
                    │     └─► Power Generation ───┼─► Wireless Systems
                    │           └─► Advanced Computing ─► Mobility
                    └─► Data Networking ──────────┬─► Advanced Production
                          └─► Logistics           └─► Wireless Systems
```

### Adding a Research Node
1. Create `scripts/research/<snake_case>.lua` with the table above
2. Add its `name` to the `requires` list of any nodes that should depend on it
3. Move component names from other nodes' `unlocks` (or add new ones) as needed
4. Move recipe names from other nodes' `unlocks_recipes` (or add new ones) as needed
5. No C++ changes needed — `ResearchManager::load()` auto-discovers all `.lua` files in the directory

## Adding a New Component (Lua)

1. Create `scripts/components/mycomponent.lua`:
```lua
return {
  name = "My Component",
  category = "Category",
  description = "What it does",
  icon = "icon-name.png",        -- from game-icons.net, white, no bg
  attributes = {
    my_attr = {
      title = "Display Name",
      type = AttributeType.FLOAT,
      value = 1.0,
    },
  },
  state = ComponentState.DEACTIVATED,
  size = ComponentSize.S,
  require = {},                   -- component types that must coexist
  conflict = {},                  -- component types that conflict
  api = {
    myMethod = function(self)
      return self.data.attributes["my_attr"]:GetFinalValue()
    end,
  },
}
```
2. CodeExecutionSystem auto-loads it on startup
3. Add inspector controls in `web/src/components/frame/ComponentControls.tsx` if needed
4. If it needs system-level logic, add handling in the relevant system's `fixedUpdate()`

## Adding a New ECS System

1. Create `include/game/systems/example.hpp`:
```cpp
#pragma once
#include <game/system.hpp>

class ExampleSystem : public System {
public:
  ExampleSystem() : System(50, "Example") {}  // 50ms interval
  void fixedUpdate() override;
};
```
2. Create `src/game/systems/example.cpp` — implement `fixedUpdate()` using EnTT registry
3. Add `systems.push_back(std::make_shared<ExampleSystem>())` in `GameManager::start()` at correct position in order

## Adding a New RPC Handler

### C++ Side
1. Create `include/rpc/handlers/mydomain_handler.hpp` with `void registerMyDomainHandlers(Server& server)`
2. Create `src/rpc/handlers/mydomain_handler.cpp`:
```cpp
server.router().on("mydomain.action", [&server](const Context& ctx, const json& params) -> json {
  requireClaim(server, ctx);                    // for mutations (omit for read-only)
  auto& gm = entt::locator<GameManager>::value();
  std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
  // ... business logic ...
  return {{"result", value}};
});
```
3. Add serialization in `include/rpc/dto.hpp` + `src/rpc/dto.cpp` if returning complex types
4. Register in `src/main.cpp`: `rpc::registerMyDomainHandlers(rpcServer);`

### Web Side
5. Add TypeScript types in `web/src/rpc/types.ts`
6. Call from store or component: `client.call("mydomain.action", { param })`

### Naming Convention
Method names: `domain.action` — e.g., `frame.create`, `component.add`, `game.pause`, `storage.set_slot`

### frame.create is Async
`frame.create` enqueues work via `gm.enqueueCommand()` and returns `{"status": "queued"}` — it cannot return errors from the creation step. `frame.create_from_blueprint` is synchronous (inline under mutex) and returns the full frame DTO. Follow the blueprint handler pattern when you need synchronous frame creation with error propagation.

## Spendable Economy

- **Pool storage**: `GameManager::spendable_pool_` (`std::map<std::string, int64_t>`). Accessors: `addSpendable(name, delta)`, `tryConsumeSpendable(cost_map, error_str)`, `spendablePool()`.
- **Cost definition**: Components/blueprints declare `spendable_cost = { ["Resource Name"] = amount }` in Lua. Parsed by `component_script_spendable_cost()` and `blueprint_spendable_total()`.
- **Blueprint total**: `blueprint_spendable_total()` sums blueprint Lua cost + all component costs. Pass its result to `tryConsumeSpendable` before creation.
- **Event**: `spendable_pool_changed_event` — emitted on every pool change, broadcast to web clients.
- **Web UI**: `SpendableBar` component shows live pool amounts. `useGameStore(s => s.spendablePool)` for access.
- **`exec->sources` key**: Keyed by the Lua `name` field verbatim (e.g. `"Near Field Communicator"` for `near_field_comminucator.lua`). Same value stored as a component's `type` attribute. Use `gm.exec->getScript(type)` to retrieve a component's Lua source by its type name.

## RPC Error Codes
- `INVALID_PARAMS` (-32602) — bad inputs
- `ENTITY_NOT_FOUND` (1002) — missing entity
- `INVALID_COMPONENT` (1003) — unknown component type
- `INTERNAL_ERROR` (-32603) — server issue
- `NOT_CLAIMED` (1000) — session not claimed for mutations

## Lua API Available in Component Scripts

### Blueprint Globals
- `frame` — owning Frame object
- `oracle` — reserved

### API-only Variables (available inside component spec `api = {}`, NOT in blueprint code)
- `frameWorld` — FrameWorld for scanAdjacent, moveFrame, nfcFrames
- `environment` — Environment (temperature, minutes, days)

### Component Dot-notation
- `comp.activate()` / `comp.deactivate()` / `comp.repair()` — state control (no self needed)
- `attr(comp, "counterpart")` — counterpart component id (-1 if none)

### comp.api Methods (injected by engine)
- `comp.api.send({destination, body, headers})` / `comp.api.read()` — data packets
- `comp.api.sendRaw(string)` / `comp.api.readRaw()` — raw data
- `comp.api.injectRaw(s)` / `comp.api.injectPacket(tbl)` — inject into own inbox
- `comp.api.queueDepthRaw()` / `comp.api.queueDepthPacket()` — check inbox
- `comp.api.getStorage()` / `comp.api.slots()` / `comp.api.slotsCount()` — storage access
- `comp.api.take(item, amount)` / `comp.api.getStackByItem(item)` — storage operations
- `comp.api.transferTo(dst, stack)` / `comp.api.transferFrom(src, stack)` — transfers

### Nexus API (through Nexus component, not global)
- `nex.api.showToast(msg, type)`, `nex.api.setGlobalIndicator(key, label, value, color)`

### Frame API
- `frame:getComponentByType("Type")` / `frame:getComponentsByType("Pattern*")`
- `frame:hasComponentType("Type")`
- `frame:getPowerInfo()` → {production, consumption, battery_count}
- `frame:getStorages()` → vector of ItemStorage

### Enums
`ComponentState.DEACTIVATED|ACTIVATING|ACTIVE|DEACTIVATING|ERROR|DESTROYED|BLOCKED|BROKEN`
`ComponentSize.S|M|L` · `FrameSize.XS|S|M|L|G` · `AttributeType.INT|FLOAT|STRING|BOOL`
`ConnectionType.POWER|DATA|CONVEYOR` · `ComponentMaterial.ALUMINIUM|COPPER|STEEL|TITANIUM|PLASTIC|GLASS`

## Web Frontend Patterns

- **Stores**: Zustand with `create<T>((set, get) => ({...}))`. Access: `useGameStore(s => s.property)`
- **Canvas**: Pixi.js via `@pixi/react`. Frames rendered as sprites with CSS overlay for text
- **Panels**: Dockview layout. Each panel takes `rpcClient` prop, uses stores
- **Styling**: Inline CSSProperties, dark theme. Shared styles in `components/ui/styles.ts`
- **ID formatting**: Use `fmtId(id)` from `components/ui/Badge.tsx` for hex IDs (`0x0A` style, min 2 digits). Use `copyToClipboard(text)` for clipboard ops. Both exported from `components/ui/index.ts`.
- **State sync**: Stores subscribe to server broadcasts (`client.on("event.state_update", ...)`)
- **Adding a panel**: Create in `components/panels/`, register in `WindowWorkspace.tsx`

## Performance Considerations

- `std::recursive_mutex` on every registry access from RPC — potential bottleneck
- Systems tick at 50ms — O(n) over all entities each tick
- WebSocket broadcasts send full state snapshots — may need delta compression
- Lua VM per frame in CodeExecutionSystem — memory grows with frame count
- Canvas renders all frames every tick — may need culling for large worlds

## Testing

- **E2E tests are the most important** — they boot the full engine and test via RPC
- Pattern: `TestHarness h; RpcClient client; client.connect(h.ws_url()); h.tick(N);`
- Always verify features work end-to-end, not just that code compiles
- Web tests: `npm test` (Vitest), `npm run test:e2e` (Playwright)
- C++ tests: `just test-e2e` (Catch2, headless engine)
