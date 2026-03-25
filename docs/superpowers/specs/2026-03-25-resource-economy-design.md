# Resource Economy System Design

**Date:** 2026-03-25
**Status:** Approved

## Overview

Introduces a draft mechanical economy: building frames and adding components costs resources (Frame Parts, Ultralight, Electronic Parts). Resources are stored in a new ECS singleton entity. Removing frames/components returns a full refund. The UI shows costs at point of action and disables actions when funds are insufficient.

---

## 1. SpendablePool ECS Entity

### C++ Struct

New `struct SpendablePool` in `include/game/components/frame.hpp` (alongside `Environment`):

```cpp
struct SpendablePool {
  std::map<std::string, int64_t> amounts;
  void field_save(FieldOutputArchive& ar) { FIELD(ar, amounts); }
  void field_load(FieldInputArchive& ar) { FIELD(ar, amounts); }
  // No cereal save/load needed — SpendablePool never appears in proto/init binary files
};
```

- Registered in `COMPONENT_LIST` macro in `include/game/component_registry.hpp`
- Added to `WellKnownEntities` as `economy` entity handle (alongside `environment`)
- Created in **both** `GameManager::loadData()` and `GameManager::start()` — WellKnownEntities is populated in both locations and the `economy` entity must be found/created in each path

### Migration from GameManager

`GameManager::spendable_pool_` (`std::map<std::string, int64_t>`) is removed. All accessors (`addSpendable`, `tryConsumeSpendable`, `spendablePool()`) are updated to read/write `SpendablePool::amounts` via the registry.

The existing save/load path also changes. `RegistryStore` currently serializes `spendable_pool` as a cereal field (written last, read only for `file_version >= 3`). Removing it shifts the binary stream, which would silently zero the pool when loading an old save. To handle this safely:

- Bump `RegistryStore::expected_version` from 3 to 4
- In `RegistryStore::load()`: add a `file_version == 3` branch that reads the old cereal `spendable_pool` field and seeds the `SpendablePool` ECS component with those values before the frame entity data is processed; skip this read on `file_version >= 4`
- Remove `spendable_pool` from `RegistryStore::save()` once version is 4
- The `Store` field passed through `Loader::saveStateToFile()` / `GameManager::saveData()` is removed
- The existing `mergeLegacySpendablesJsonInto` migration path (for save version < 3) must populate the ECS component instead of the removed `spendable_pool_` field

The existing event/broadcast plumbing (`spendable_pool_changed_event`, state snapshot serialization of the pool) stays unchanged — only the backing storage changes.

---

## 2. Frame Size Costs

Defined as a C++ static map in `include/game/frame_costs.hpp`, mirrored as a TypeScript constant on the frontend:

| Frame Size | Cost |
|------------|------|
| XS         | 10 Ultralight |
| S          | 25 Frame Parts |
| M          | 50 Frame Parts |
| L          | 100 Frame Parts |
| G          | 50 Frame Parts + 50 Ultralight |

### Application Points

**`frame.create`**: This handler currently enqueues creation asynchronously and returns `{"status": "queued"}`. Convert it to execute inline under the mutex (as `frame.create_from_blueprint` already does — `updateMutex` is `recursive_mutex` so nested locks are safe). Check and deduct frame size cost before creating. Returns `INVALID_PARAMS` error with message (e.g. `"Not enough Frame Parts"`) if pool is short. After conversion the handler returns the full serialized frame DTO (matching `frame.create_from_blueprint`). Size param defaults to S if omitted.

**`frame.create_from_blueprint`**: `blueprint_spendable_total()` is extended to include the frame size cost. Frame size is read from `bp_spec["size"]` (already accessible in `addFrameFromBlueprint`) and looked up in the C++ cost map. The merged total (blueprint Lua cost + component costs + frame size cost) is passed to the single `tryConsumeSpendable` check.

**`frame.remove`**: Before `registry.destroy()`, refund frame size cost + sum of all component spendable costs (see Section 5).

---

## 3. Component Costs

Each component Lua file gets `spendable_cost = { ["Electronic Parts"] = N }` based on its `size`:

| Component Size | Electronic Parts |
|----------------|-----------------|
| S              | 10              |
| M              | 25              |
| L              | 50              |

### Component → Size → Cost Mapping

**S (10 EP):** charger, clock, control_relay, conveyor_connector, conveyor_relay, core, data_relay, data_wire_connector, data_wireless_emitter, data_wireless_receiver, lidar, load_controller, main_core, near_field_comminucator, power_meter, power_wire_connector, power_wireless_emitter, power_wireless_receiver, propulsion, temperature_sensor

**M (25 EP):** battery, big_storage, capacitor, consumer, cooler, external_aoe_cooler, external_aoe_heater, heater, life_support, miner, packer, refinery, storage

**L (50 EP):** generator, heat_sink, nexus, solar_panel

### Application Points

**`component.add`**: Already checks and deducts via `component_script_spendable_cost()` — no change needed.

**`component.remove`**: The handler has the component's `type` attribute string (e.g. `"Charger"`, `"Near Field Communicator"`). `exec->sources` is keyed by the Lua `name` field verbatim (e.g. `"Near Field Communicator"` for `near_field_comminucator.lua`), which is the same value stored as the `type` attribute. Use `gm.exec->getScript(type)` to get the Lua source and parse `spendable_cost`, then refund via `addSpendable`.

---

## 4. Frontend — Cost Display

All costs are shown at point of action. A static `FRAME_COSTS` constant in TypeScript mirrors the C++ map (no extra RPC needed). Component costs come from the component spec already returned by the server.

### Frame Creation Dialog

- Size selector shows cost next to each size option: `"Small — 25 Frame Parts"`
- Sizes the player cannot afford are greyed out / disabled

### Component Add

- Each component card shows its cost: `"10 Electronic Parts"`
- Add button is disabled (with tooltip) if the pool is insufficient

### Blueprint Creation

- Blueprint card shows total cost (frame size cost + all component costs summed)
- Create button disabled if insufficient

### SpendableBar

No change — already shows live pool amounts.

---

## 5. Refund Policy

All refunds are **full (100%)** — draft economy.

| Action | Refund |
|--------|--------|
| `component.remove` | Component's `spendable_cost` (read from Lua source via `type` key) |
| `frame.remove` | Frame size cost + all components' `spendable_cost` summed |

For `frame.remove`: iterate `frame.components` before destroy, accumulate all cost maps, add the frame size cost, call `addSpendable` once with the merged total, then `registry.destroy`.

---

## 6. Error Handling

Use existing `INVALID_PARAMS` (-32602) with a human-readable message (e.g. `"Not enough Frame Parts"`) for failed cost checks — consistent with current `tryConsumeSpendable` behavior. No new error codes needed.

---

## 7. Out of Scope

- Resource generation rates / income mechanics
- Partial refunds or durability
- Pricing configuration UI (prices are hardcoded for this draft)
- Migration of existing save files (existing frames keep their components for free — acceptable for draft)
