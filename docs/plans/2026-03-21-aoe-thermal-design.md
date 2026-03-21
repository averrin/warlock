# AoE External Cooler / Heater and Temperature Field (Design)

**Date:** 2026-03-21  
**Status:** Approved (with visualization scope)

## Summary

Add **external** thermal devices (cooler / heater) modeled as frame components with an integer **radius** in **world grid cells**. Influence uses a **2D Euclidean** distance from a defined **anchor** on the source frame to each affected **cell**, with a **smooth falloff** to zero at the radius edge. **Stacking:** contributions from multiple devices **sum** (optional global cap is a later tuning knob).

**Airflow:** Phase 1 scales each source’s contribution using **`environment.airFlow`** (same family as existing convection scaling in `ThermalSystem`). Phase 2 introduces **`effective_airflow(gx, gy)`** that defaults to `environment.airFlow` and can later incorporate **per-cell** airflow from a future external airflow device.

**Thermal effect:** Apply a **per-frame bias** during thermal integration for frames that overlap the influenced region (components in a frame share the same bias in phase 1; per-cell asymmetry inside a large frame is out of scope unless added later).

## Spatial model

- Grid matches existing **`kCellPx` (25)** and `frameOccupiedGridCells`-style cell indices.
- **Source anchor:** Document one rule (e.g. centroid of the source frame’s occupied cells, or top-left cell center) and use it consistently for distance and debug overlay.
- **Radius `R` (int):** Include cells whose **center** lies within **Euclidean distance ≤ R** from the anchor (exact comparison boundary documented in implementation).

## Falloff

- Use a **smooth** bounded weight on `[0, R]`, e.g. quadratic `max(0, 1 - (d/R)^2)` or **smoothstep**, so strength is not a hard binary disk.

## Device metadata

- Component `type` distinguishes external cooler vs heater (or a signed `strength` / `delta` attribute).
- **`radius`:** int (grid cells).
- **Active** gating consistent with other components (`ComponentState::ACTIVE`, etc.).

## Toggleable temperature gradient visualization (map)

**Goal:** Let players and developers **see** the **spatial thermal field** on the **world map** (web canvas), aligned with the same grid as frames and patches.

**Behavior:**

- **Toggle:** A **single UI control** (e.g. toolbar or view menu) **“Show temperature field”** / **“Heat map”** that turns the overlay **on** and **off**. Optional: persist the choice in **`localStorage`** so it survives reloads.
- **When off:** No extra RPC or minimal traffic (no continuous heatmap stream unless already required for other features).
- **When on:** Client receives enough data to draw a **semi-transparent** **per-cell** (or batched) overlay: color encodes a **normalized scalar field** (e.g. combined **environment baseline + summed AoE influence** for the tick, or a dedicated **debug influence** value—implementation picks one scalar and documents it). Use a clear **cold→hot** colormap (e.g. blue → neutral → red) with **alpha** so frames/patches remain readable.
- **Layering:** Render in **Pixi** in a **dedicated layer** (order: consistent with existing grid/patch/frame stack—typically **above patches**, **below or beside** frame HTML overlays so labels stay clickable; exact z-order in implementation plan).
- **Performance:** Only paint **visible** cells (viewport culling); consider **coarse subsampling** at extreme zoom-out if needed.

**Authority:** Field values should match **server simulation** definitions (client does not invent physics). The server either **pushes** a bounded snapshot when the client enables the toggle or answers an **on-demand** query for the current field over a **rectangle** of cells.

## Testing

- **Unit / integration:** Distance, falloff weights, stacking, and frame overlap selection.
- **E2E / UI:** Toggle persists overlay visibility; with mock or fixed field, colors appear only when toggled on.

## Out of scope (later)

- Per-cell airflow device and **`effective_airflow(gx, gy)`** beyond a stub.
- Per-component positions inside a frame for intra-frame gradient.
- Full fluid simulation.
