# AoE Thermal Devices + Temperature Field Visualization Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement external cooler/heater AoE thermal influence on the world grid (smooth falloff, airflow scaling, per-frame bias), and a **toggleable** map overlay that visualizes the resulting **temperature / influence field** on the web canvas.

**Architecture:** Extend `ThermalSystem` (or a small helper used by it) to accumulate contributions from active external thermal components using shared grid math (`frameOccupiedGridCells`, Euclidean distance from a fixed frame anchor). Expose a **server-authoritative** scalar field for the **visible region** or full bounded map via RPC when the client enables **heatmap mode**. Web: `GameCanvas.tsx` adds a Pixi **Graphics** (or similar) layer, driven by store state and RPC payloads; a **toolbar toggle** controls visibility and optionally subscribes to field updates.

**Tech Stack:** C++17, entt, existing RPC/WebSocket, TypeScript, React, PixiJS (`web/`).

---

## Task 1: Grid math helpers (anchor + distance)

**Files:**
- Create: `include/game/thermal_grid.hpp` (or extend `include/game/frame_deposit_query.hpp` if you prefer fewer files)
- Modify: `tests/unit/` — add test file if a unit test target exists for pure functions

**Step 1:** Implement **frame anchor** (centroid of occupied grid cells from `wl::transform` + `FrameSize`) and **cell centers** for overlap tests.

**Step 2:** Implement **weight(d, R)** with smooth falloff (quadratic or smoothstep), `d` = Euclidean distance in **cell space**.

**Step 3:** Run existing tests + new tests.

**Step 4: Commit**

```bash
git add include/game/thermal_grid.hpp tests/unit/test_thermal_grid.cpp
git commit -m "feat(thermal): add grid anchor and AoE falloff helpers"
```

---

## Task 2: Component types and Lua/JSON definitions

**Files:**
- Modify: component blueprints or Lua scripts where new types are registered (mirror existing `"Temp Control"` pattern)
- Document: `radius` (int), sign/strength attributes for cooler vs heater

**Step 1:** Add **external cooler** and **external heater** (or one type with signed `strength`) with required metadata.

**Step 2:** Commit.

---

## Task 3: ThermalSystem — accumulate AoE and apply per-frame bias

**Files:**
- Modify: `src/game/systems/thermal.cpp`
- Modify: `include/game/systems/thermal.hpp` if new private helpers are needed

**Step 1:** On each `fixedUpdate`, collect all active external thermal components (by `type`), compute their influence on **other frames** whose `frameOccupiedGridCells` intersect the **weighted** influence region (within `R`).

**Step 2:** Apply **airflow factor** `f_air = a0 + a1 * environment->airFlow` (tune constants; keep in one place).

**Step 3:** Add **per-frame** extra term to heat transfer or equivalent bias (match existing `Temp Control` style where possible for consistency).

**Step 4:** Add stub `effective_airflow(gx, gy)` returning `environment->airFlow` for future per-cell use.

**Step 5:** Run `ctest` / project test command.

**Step 6: Commit**

```bash
git add src/game/systems/thermal.cpp include/game/systems/thermal.hpp
git commit -m "feat(thermal): AoE external cooler/heater with falloff and airflow"
```

---

## Task 4: Server — field snapshot for visualization

**Files:**
- Modify: `src/rpc/handlers/state_handler.cpp` or `game_handler.cpp` (follow existing env/state patterns)
- Modify: `src/rpc/dto.cpp` if new DTO fields are needed
- Modify: `include/game/components/frame.hpp` only if serialization needs extension (avoid if unnecessary)

**Step 1:** Define a **scalar field** over grid cells for one tick: same formula the overlay should display (document in handler: e.g. **influence sum** + **environment.temperature** offset, or pure **delta**—pick one and keep stable).

**Step 2:** Add RPC method or extend `state` push: e.g. **`state.thermalField`** with `{ minGx, minGy, width, height, values: number[] }** or **sparse** `{ cells: [{x,y,v}] }** if most cells are zero. Prefer **rectangular** window for viewport queries.

**Step 3:** When no client has heatmap enabled, skip heavy work (guard with subscription or explicit pull).

**Step 4:** Add or extend **e2e** test asserting JSON shape when method is called.

**Step 5: Commit**

```bash
git add src/rpc/handlers/ src/rpc/dto.cpp
git commit -m "feat(rpc): thermal field snapshot for map overlay"
```

---

## Task 5: Web — RPC client and types

**Files:**
- Modify: `web/src/rpc/client.ts` or equivalent
- Modify: `web/src/rpc/types.ts` if present

**Step 1:** Add method typings and parser for thermal field payload.

**Step 2:** Commit.

---

## Task 6: Web — store toggle + optional localStorage

**Files:**
- Modify: `web/src/stores/game.ts` (or a small `web/src/stores/view.ts`)

**Step 1:** State: **`showThermalField: boolean`**, actions **`setShowThermalField`**.

**Step 2:** Optional: **`localStorage`** key e.g. `warlock.showThermalField` read on init, write on toggle.

**Step 3:** Commit.

---

## Task 7: Web — Pixi overlay layer in GameCanvas

**Files:**
- Modify: `web/src/components/canvas/GameCanvas.tsx`

**Step 1:** Create **`thermalFieldLayerRef`** (`Graphics` or `Mesh`**)** inserted in the **world** container at a fixed **z-index** (document order vs `patchLayer`, `grid`, `markerLayer`).

**Step 2:** On **`showThermalField`** and when **field data** updates, **clear** and **fill rects** per cell (`CELL` size) for **viewport-intersecting** cells only; map value → **color** (blue–red) and **alpha** ~0.25–0.45.

**Step 3:** On pan/zoom/resize, **redraw** overlay (reuse existing redraw hooks / `useEffect` dependencies).

**Step 4:** When toggle off, **hide** layer and **stop** requesting field updates (if using polling/subscription).

**Step 5:** Manual check: toggle shows/hides overlay; frames remain usable.

**Step 6: Commit**

```bash
git add web/src/components/canvas/GameCanvas.tsx web/src/stores/game.ts
git commit -m "feat(web): toggleable thermal field overlay on map"
```

---

## Task 8: Toolbar / UI control

**Files:**
- Modify: appropriate layout component under `web/src/components/` (toolbar next to zoom or existing view controls)

**Step 1:** Add **checkbox or button** bound to **`showThermalField`**.

**Step 2:** Accessibility: `aria-pressed` or label **“Temperature field”**.

**Step 3:** Commit.

---

## Task 9: Verification

**Commands (adjust to repo CMake/Makefile):**

```bash
cmake --build build && ctest --test-dir build
cd web && npm test
```

**Expected:** All tests green; manual: toggle overlay, field matches movement of frames with devices.

**Step 1:** Run full suite; fix failures.

**Step 2:** Final commit if fixes needed.

---

## Notes

- **YAGNI:** Stub `effective_airflow` only; no per-cell airflow device in this milestone.
- **DRY:** Share falloff math between simulation and field snapshot (same C++ helper).
- **TDD:** Prefer a unit test for **weight(d,R)** and **distance** before wiring `ThermalSystem`.
