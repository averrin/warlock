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
};
```

- Registered in `COMPONENT_LIST` macro in `include/game/component_registry.hpp`
- Added to `WellKnownEntities` as `economy` entity (alongside `environment`)
- Created in `GameManager::start()` — new entity with `SpendablePool` component attached

### Migration

`GameManager::spendable_pool_` (`std::map<std::string, int64_t>`) is removed. All accessors (`addSpendable`, `tryConsumeSpendable`, `spendablePool()`) are updated to read/write `SpendablePool::amounts` via the registry. The existing event/broadcast plumbing (`spendable_pool_changed_event`, state snapshot serialization) stays unchanged — only the backing storage changes.

---

## 2. Frame Size Costs

Defined as a C++ static map in `include/game/frame_costs.hpp` (or inline utility), mirrored as a TypeScript constant on the frontend:

| Frame Size | Cost |
|------------|------|
| XS         | 10 Ultralight |
| S          | 25 Frame Parts |
| M          | 50 Frame Parts |
| L          | 100 Frame Parts |
| G          | 50 Frame Parts + 50 Ultralight |

### Application Points

- **`frame.create`**: Check and deduct frame size cost before creating. Returns `INSUFFICIENT_FUNDS` error if pool is short. Size param defaults to S if omitted.
- **`frame.create_from_blueprint`**: `blueprint_spendable_total()` extended to include the frame size cost from the C++ map, in addition to existing blueprint Lua cost + component costs.
- **`frame.remove`**: Before `registry.destroy()`, refund frame size cost + sum of all component spendable costs.

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

- **`component.add`**: Already checks and deducts — no change needed.
- **`component.remove`**: Read `spendable_cost` from the component's Lua source and refund via `addSpendable`.

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
| `component.remove` | Component's `spendable_cost` |
| `frame.remove` | Frame size cost + all components' costs |

For `frame.remove`: iterate `frame.components` before destroy, accumulate costs, call `addSpendable` once with the total map, then `registry.destroy`.

---

## 6. Error Handling

Use existing `INSUFFICIENT_FUNDS` (or `INVALID_PARAMS` with message) error code for failed cost checks — consistent with current `tryConsumeSpendable` behavior. No new error codes needed.

---

## 7. Out of Scope

- Resource generation rates / income mechanics
- Partial refunds or durability
- Pricing configuration UI (prices are hardcoded for this draft)
- Migration of existing save files (existing frames get their components for free — acceptable for draft)
