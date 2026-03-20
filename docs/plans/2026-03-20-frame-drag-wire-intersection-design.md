# Frame drag: block intersection with WIRE-medium connections

## Goal

While dragging one or more frames, reject candidate positions that would cause **any moved frame’s axis-aligned rectangle** (same cell sizing as today) to intersect **any straight wire segment** drawn for connections whose **medium is `WIRE`** (including `medium` omitted — default `WIRE`).

Wireless and beam connections are **not** part of this check (they may still be drawn; movement is not constrained by them).

## Current behavior

- `useFrameDrag.ts` applies `isValidGroupMove` → overlap only between frame bounds.
- `GameCanvas.tsx` `drawConnections` draws segments between frame centers with perpendicular offsets when multiple **types** share the same frame pair (`TYPE_ORDER`, `OFFSET_STEP`).

## Geometry rule (must match the canvas)

Group connections by undirected frame pair (same as `drawConnections`). For each pair, compute `presentTypes` = POWER/DATA/CONVEYOR that appear in **any** connection on that pair (any medium), because perpendicular **offset indices** depend on the full set of types drawn.

For each **connection** `c` with `(c.medium ?? "WIRE") === "WIRE"`:

1. Resolve `source` / `target` positions from the **candidate** move map when those frames are being dragged, otherwise `framePositionsRef` / `framesRef` fallbacks (same as overlap check).
2. Compute segment endpoints **identically** to `drawConnections`: center points, `nx`/`ny` from the chord, offset using the **index of `c.type` inside `presentTypes`** for that pair (not “WIRE-only types”), so a WIRE POWER line stays aligned when a DATA line on the same pair is WIRELESS.
3. For each **other** frame `f` (`f` not equal to `source` or `target` of that segment): if the world-space rectangle of `f` (using candidate position when `f` is in the move) intersects that segment, the move is **invalid**.

**Endpoint exclusion:** Segments always terminate at frame centers; endpoint frames must not be tested against their own segment (`f !== source && f !== target`), or every frame would trivially “hit” its incident wires.

## Approaches

| Option | Description | Pros | Cons |
|--------|-------------|------|------|
| **A. Shared segment builder + segment–AABB test in `useFrameDrag`** | Pass `connectionsRef` + reuse `CELL` / `FRAME_CELL_SIZES` (or import shared constants). One function builds wire segments for WIRE-only; second tests each candidate frame rect vs segments not incident to that frame. | Single source of truth if segment math is extracted to a small shared module used by `drawConnections` and drag (ideal). | Requires refactor or careful duplication until shared. |
| **B. Duplicate segment math inside `isValidGroupMove` only** | Copy offset logic from `drawConnections` into drag validation. | Fastest to ship; no `drawConnections` edit. | Drift risk if drawing changes. |
| **C. Rasterize / grid sampling** | Sample points along segments. | Avoids line–rect math. | Harder to match offsets; slower; easy to get wrong. |

**Recommendation:** **A** — extract `buildConnectionSegments(connections, positions, frames, opts?: { medium?: 'WIRE' })` (or WIRE-filtered internally) into a small `web/src/components/canvas/connectionGeometry.ts` (or next to `useFrameDrag`), and call it from `drawConnections` and from `isValidGroupMove`. If extraction is too large for a minimal PR, **B** as an interim with a comment pointing to `drawConnections` is acceptable.

## Edge cases

- **Group drag:** All candidate positions applied together; segments use candidate positions for all frames in the group.
- **Multiple connections same pair:** Offsets follow the **full** type set on that pair; only WIRE-medium connections get a segment tested against frames.
- **Non-WIRE media:** Ignored for blocking; no change to drag behavior relative to them.

## Testing

- Unit-test segment–rectangle intersection helper (corners, parallel, endpoint near miss).
- Optional: snapshot-style test that given fixed frames/connections, a move that crosses a third frame’s wire is rejected.

## Next step

Implementation plan via `writing-plans` skill after this design is accepted.
