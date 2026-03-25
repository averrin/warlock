/**
 * Single world grid: one cell = {@link CELL} world units (px).
 * Frame sizes are in **cells along one side**; area in cells = side² (e.g. S = 3×3 = 9 cells).
 */
export const CELL = 25;

/** Cells per side of the frame square (world size = count × {@link CELL}). */
export const FRAME_CELL_SIZES: Record<string, number> = {
  XS: 1,
  S: 3,
  M: 6,
  L: 9,
  G: 12,
};

/**
 * Snapped top-left positions (aligned to {@link CELL}) such that the frame square
 * overlaps the grid cell at integer indices `(ix, iy)` (world rect origin `(ix*CELL, iy*CELL)`, size `CELL`).
 */
export function snappedTopLeftsOverlappingCell(
  ix: number,
  iy: number,
  side: number,
): Array<{ x: number; y: number }> {
  const out: Array<{ x: number; y: number }> = [];
  const tx0 = Math.floor((ix * CELL - side) / CELL) * CELL;
  const tx1 = Math.ceil(((ix + 1) * CELL) / CELL) * CELL;
  const ty0 = Math.floor((iy * CELL - side) / CELL) * CELL;
  const ty1 = Math.ceil(((iy + 1) * CELL) / CELL) * CELL;
  for (let tx = tx0; tx < tx1; tx += CELL) {
    if (!(tx < (ix + 1) * CELL && tx + side > ix * CELL)) continue;
    for (let ty = ty0; ty < ty1; ty += CELL) {
      if (!(ty < (iy + 1) * CELL && ty + side > iy * CELL)) continue;
      out.push({ x: tx, y: ty });
    }
  }
  return out;
}

export const CONN_TYPE_ORDER = ["POWER", "DATA", "CONVEYOR"] as const;
export type ConnTypeOrder = (typeof CONN_TYPE_ORDER)[number];

/** Perpendicular spacing between parallel links between the same frame pair (must exceed socket diameter for ports). */
export const OFFSET_STEP = 10;

/** World-space axis-aligned frame square from top-left origin. */
export interface FrameRect {
  x: number;
  y: number;
  w: number;
  h: number;
}

export function frameRectFromTopLeft(origin: { x: number; y: number }, sizeKey: string | undefined): FrameRect {
  const cells = FRAME_CELL_SIZES[sizeKey ?? "S"] ?? 1;
  const side = cells * CELL;
  return { x: origin.x, y: origin.y, w: side, h: side };
}

export function frameCenterFromTopLeft(origin: { x: number; y: number }, sizeKey: string | undefined): { x: number; y: number } {
  const r = frameRectFromTopLeft(origin, sizeKey);
  return { x: r.x + r.w / 2, y: r.y + r.h / 2 };
}

/** Grid cell keys `"ix,iy"` for patches with `obstacle: true`. */
export function obstacleCellKeysFromPatches(
  patches: readonly { obstacle?: boolean; cells: readonly [number, number][] }[],
): Set<string> {
  const s = new Set<string>();
  for (const p of patches) {
    if (!p.obstacle) continue;
    for (const [cx, cy] of p.cells) {
      s.add(`${cx},${cy}`);
    }
  }
  return s;
}

/** Integer grid cells covered by a frame's top-left origin (same convention as server frame grid). */
export function frameOccupiedCellKeys(origin: { x: number; y: number }, sizeKey: string | undefined): Set<string> {
  const side = FRAME_CELL_SIZES[sizeKey ?? "S"] ?? 1;
  const gx = Math.floor(origin.x / CELL);
  const gy = Math.floor(origin.y / CELL);
  const s = new Set<string>();
  for (let dy = 0; dy < side; dy++) {
    for (let dx = 0; dx < side; dx++) {
      s.add(`${gx + dx},${gy + dy}`);
    }
  }
  return s;
}

/**
 * Point where a ray from `origin` (inside `rect`) in direction `dir` (unit) exits the rectangle.
 */
export function rayExitFromRect(origin: { x: number; y: number }, dir: { x: number; y: number }, rect: FrameRect): { x: number; y: number } {
  const left = rect.x;
  const right = rect.x + rect.w;
  const top = rect.y;
  const bottom = rect.y + rect.h;
  const eps = 1e-9;
  let tMin = Infinity;

  const consider = (t: number, onEdge: () => boolean) => {
    if (t > eps && onEdge()) tMin = Math.min(tMin, t);
  };

  if (dir.x > eps) {
    const t = (right - origin.x) / dir.x;
    const py = origin.y + t * dir.y;
    consider(t, () => py >= top - eps && py <= bottom + eps);
  } else if (dir.x < -eps) {
    const t = (left - origin.x) / dir.x;
    const py = origin.y + t * dir.y;
    consider(t, () => py >= top - eps && py <= bottom + eps);
  }

  if (dir.y > eps) {
    const t = (bottom - origin.y) / dir.y;
    const px = origin.x + t * dir.x;
    consider(t, () => px >= left - eps && px <= right + eps);
  } else if (dir.y < -eps) {
    const t = (top - origin.y) / dir.y;
    const px = origin.x + t * dir.x;
    consider(t, () => px >= left - eps && px <= right + eps);
  }

  if (tMin === Infinity) return { x: origin.x, y: origin.y };
  return { x: origin.x + tMin * dir.x, y: origin.y + tMin * dir.y };
}

/**
 * Straight segment between frame perimeters along the line connecting frame centers:
 * exit point on the source rect toward the target, entry point on the target rect from the source.
 */
export function edgeSegmentBetweenFrameCenters(
  srcPos: { x: number; y: number },
  srcSize: string | undefined,
  tgtPos: { x: number; y: number },
  tgtSize: string | undefined,
): { x1: number; y1: number; x2: number; y2: number } {
  const ra = frameRectFromTopLeft(srcPos, srcSize);
  const rb = frameRectFromTopLeft(tgtPos, tgtSize);
  const c1 = { x: ra.x + ra.w / 2, y: ra.y + ra.h / 2 };
  const c2 = { x: rb.x + rb.w / 2, y: rb.y + rb.h / 2 };
  const dx = c2.x - c1.x;
  const dy = c2.y - c1.y;
  const len = Math.sqrt(dx * dx + dy * dy) || 1;
  const d = { x: dx / len, y: dy / len };
  const p1 = rayExitFromRect(c1, d, ra);
  const p2 = rayExitFromRect(c2, { x: -d.x, y: -d.y }, rb);
  return { x1: p1.x, y1: p1.y, x2: p2.x, y2: p2.y };
}

/** Wiring preview: socket on source frame edge toward `toward` (e.g. cursor). */
export function edgeExitTowardPoint(
  srcPos: { x: number; y: number },
  srcSize: string | undefined,
  toward: { x: number; y: number },
): { x: number; y: number } {
  const ra = frameRectFromTopLeft(srcPos, srcSize);
  const c = { x: ra.x + ra.w / 2, y: ra.y + ra.h / 2 };
  const dx = toward.x - c.x;
  const dy = toward.y - c.y;
  const len = Math.sqrt(dx * dx + dy * dy) || 1;
  const d = { x: dx / len, y: dy / len };
  return rayExitFromRect(c, d, ra);
}

export interface ConnectionLike {
  source: number;
  target: number;
  type: string;
  medium?: string;
  transfer_progress?: number;
}

/** All connections between the same unordered frame pair (min/max id). */
export function connectionsForOrderedPair(
  connections: readonly ConnectionLike[],
  idA: number,
  idB: number,
): ConnectionLike[] {
  const lo = Math.min(idA, idB);
  const hi = Math.max(idA, idB);
  return connections.filter((c) => Math.min(c.source, c.target) === lo && Math.max(c.source, c.target) === hi);
}

export function groupConnectionsByPair(
  connections: readonly ConnectionLike[],
): Map<string, ConnectionLike[]> {
  const groups = new Map<string, ConnectionLike[]>();
  for (const conn of connections) {
    const a = Math.min(conn.source, conn.target);
    const b = Math.max(conn.source, conn.target);
    const key = `${a}-${b}`;
    let arr = groups.get(key);
    if (!arr) {
      arr = [];
      groups.set(key, arr);
    }
    arr.push(conn);
  }
  return groups;
}

/**
 * Straight segment between frame centers with the same perpendicular offset as `drawConnections`,
 * for one connection type on a pair. Returns null if the type is not drawn for that pair.
 */
export function segmentForPairAndType(
  connsForPair: readonly ConnectionLike[],
  type: string,
  getPosition: (frameId: number) => { x: number; y: number } | null,
  getFrameSize: (frameId: number) => string | undefined,
): { x1: number; y1: number; x2: number; y2: number } | null {
  const sample = connsForPair[0];
  if (!sample) return null;

  const srcPos = getPosition(sample.source);
  const tgtPos = getPosition(sample.target);
  const srcSize = getFrameSize(sample.source);
  const tgtSize = getFrameSize(sample.target);
  if (!srcPos || !tgtPos || srcSize === undefined || tgtSize === undefined) return null;

  const presentTypes = CONN_TYPE_ORDER.filter((t) => connsForPair.some((c) => c.type === t));
  const idx = presentTypes.indexOf(type as ConnTypeOrder);
  if (idx < 0) return null;

  const edge = edgeSegmentBetweenFrameCenters(srcPos, srcSize, tgtPos, tgtSize);
  const baseX1 = edge.x1;
  const baseY1 = edge.y1;
  const baseX2 = edge.x2;
  const baseY2 = edge.y2;

  const dx = baseX2 - baseX1;
  const dy = baseY2 - baseY1;
  const len = Math.sqrt(dx * dx + dy * dy) || 1;
  const nx = -dy / len;
  const ny = dx / len;

  const count = presentTypes.length;
  const startIndex = -(count - 1) / 2;
  const offset = (startIndex + idx) * OFFSET_STEP;
  const ox = nx * offset;
  const oy = ny * offset;

  return {
    x1: baseX1 + ox,
    y1: baseY1 + oy,
    x2: baseX2 + ox,
    y2: baseY2 + oy,
  };
}

const MEDIUM_ORDER: Record<string, number> = { WIRE: 0, BEAM: 1, WIRELESS: 2 };

/** Wire segment for a connection that is not yet in `existingPairConns` (same pair, merged then sorted). */
export function segmentForHypotheticalConnection(
  existingPairConns: readonly ConnectionLike[],
  newConn: ConnectionLike,
  getPosition: (frameId: number) => { x: number; y: number } | null,
  getFrameSize: (frameId: number) => string | undefined,
): { x1: number; y1: number; x2: number; y2: number } | null {
  const merged = [...existingPairConns, newConn];
  const sorted = sortConnectionsForPair(merged);
  const idx = sorted.findIndex(
    (c) =>
      c.source === newConn.source &&
      c.target === newConn.target &&
      c.type === newConn.type &&
      (c.medium ?? "WIRE") === (newConn.medium ?? "WIRE"),
  );
  const i = idx >= 0 ? idx : sorted.length - 1;
  return segmentForPairNthConnection(sorted, i, getPosition, getFrameSize);
}

/**
 * Center-distance limit plus WIRE/BEAM obstruction using the same edge-based segment as drawing.
 * Caller supplies `centerDist` and `effectiveMax` from frame centers and connector ranges.
 */
export function connectionWiringGeometryOk(
  sourceId: number,
  targetId: number,
  wType: string,
  wMedium: string,
  effectiveMax: number,
  centerDist: number,
  currentFrames: readonly { id: number; size: string }[],
  currentConns: readonly ConnectionLike[],
  getPosition: (id: number) => { x: number; y: number } | null,
  getFrameSize: (id: number) => string | undefined,
  obstacleCells?: Set<string>,
): boolean {
  if (centerDist > effectiveMax) return false;
  if (wMedium !== "WIRE" && wMedium !== "BEAM") return true;
  const newConn = { source: sourceId, target: targetId, type: wType, medium: wMedium } as ConnectionLike;
  const pairConns = connectionsForOrderedPair(currentConns, sourceId, targetId);
  const seg = segmentForHypotheticalConnection(pairConns, newConn, getPosition, getFrameSize);
  if (!seg) return true;
  for (const other of currentFrames) {
    if (other.id === sourceId || other.id === targetId) continue;
    const oPos = getPosition(other.id);
    if (!oPos) continue;
    const side = (FRAME_CELL_SIZES[other.size] ?? 1) * CELL;
    if (
      segmentIntersectsAxisAlignedRect(
        seg.x1,
        seg.y1,
        seg.x2,
        seg.y2,
        oPos.x,
        oPos.y,
        side,
        side,
      )
    ) {
      return false;
    }
  }
  if (obstacleCells && obstacleCells.size > 0) {
    for (const key of obstacleCells) {
      const [ix, iy] = key.split(",").map(Number);
      if (
        segmentIntersectsAxisAlignedRect(
          seg.x1,
          seg.y1,
          seg.x2,
          seg.y2,
          ix * CELL,
          iy * CELL,
          CELL,
          CELL,
        )
      ) {
        return false;
      }
    }
  }
  return true;
}

/** Sort connections for stable parallel offsets (same pair). */
export function sortConnectionsForPair(conns: readonly ConnectionLike[]): ConnectionLike[] {
  return [...conns].sort((a, b) => {
    const ta = CONN_TYPE_ORDER.indexOf(a.type as ConnTypeOrder);
    const tb = CONN_TYPE_ORDER.indexOf(b.type as ConnTypeOrder);
    if (ta !== tb) return ta - tb;
    const ma = MEDIUM_ORDER[a.medium ?? "WIRE"] ?? 0;
    const mb = MEDIUM_ORDER[b.medium ?? "WIRE"] ?? 0;
    return ma - mb;
  });
}

/**
 * Same perpendicular offset layout as {@link segmentForPairAndType}, but one line per
 * connection (handles multiple links of the same type between the same pair).
 */
export function segmentForPairNthConnection(
  sortedConns: readonly ConnectionLike[],
  index: number,
  getPosition: (frameId: number) => { x: number; y: number } | null,
  getFrameSize: (frameId: number) => string | undefined,
): { x1: number; y1: number; x2: number; y2: number } | null {
  const sample = sortedConns[index];
  if (!sample) return null;

  const srcPos = getPosition(sample.source);
  const tgtPos = getPosition(sample.target);
  const srcSize = getFrameSize(sample.source);
  const tgtSize = getFrameSize(sample.target);
  if (!srcPos || !tgtPos || srcSize === undefined || tgtSize === undefined) return null;

  const edge = edgeSegmentBetweenFrameCenters(srcPos, srcSize, tgtPos, tgtSize);
  const baseX1 = edge.x1;
  const baseY1 = edge.y1;
  const baseX2 = edge.x2;
  const baseY2 = edge.y2;

  const dx = baseX2 - baseX1;
  const dy = baseY2 - baseY1;
  const len = Math.sqrt(dx * dx + dy * dy) || 1;
  const nx = -dy / len;
  const ny = dx / len;

  const count = sortedConns.length;
  const startIndex = -(count - 1) / 2;
  const offset = (startIndex + index) * OFFSET_STEP;
  const ox = nx * offset;
  const oy = ny * offset;

  return {
    x1: baseX1 + ox,
    y1: baseY1 + oy,
    x2: baseX2 + ox,
    y2: baseY2 + oy,
  };
}

/** Segment [p1,p2] vs closed axis-aligned rectangle (inclusive bounds). */
export function segmentIntersectsAxisAlignedRect(
  x1: number,
  y1: number,
  x2: number,
  y2: number,
  rx: number,
  ry: number,
  rw: number,
  rh: number,
): boolean {
  const minX = rx;
  const minY = ry;
  const maxX = rx + rw;
  const maxY = ry + rh;

  let t0 = 0;
  let t1 = 1;
  const dx = x2 - x1;
  const dy = y2 - y1;

  const clip = (p: number, q: number): boolean => {
    if (p === 0) return q >= 0;
    const r = q / p;
    if (p < 0) {
      if (r > t1) return false;
      if (r > t0) t0 = r;
    } else {
      if (r < t0) return false;
      if (r < t1) t1 = r;
    }
    return true;
  };

  if (!clip(-dx, x1 - minX)) return false;
  if (!clip(dx, maxX - x1)) return false;
  if (!clip(-dy, y1 - minY)) return false;
  if (!clip(dy, maxY - y1)) return false;
  return t0 <= t1;
}

export function groupMoveAvoidsWireIntersections(
  moves: Map<number, { x: number; y: number }>,
  connections: readonly ConnectionLike[],
  frames: readonly { id: number; size: string; position?: { x: number; y: number } }[],
  framePositions: ReadonlyMap<number, { x: number; y: number }>,
  obstacleCells?: Set<string>,
): boolean {
  const getPosition = (id: number): { x: number; y: number } | null => {
    const m = moves.get(id);
    if (m) return m;
    const p = framePositions.get(id);
    if (p) return p;
    const f = frames.find((x) => x.id === id);
    return f?.position ? { x: f.position.x, y: f.position.y } : null;
  };

  const getFrameSize = (id: number) => frames.find((f) => f.id === id)?.size;

  const groups = groupConnectionsByPair(connections);
  const wireSegments: Array<{
    x1: number;
    y1: number;
    x2: number;
    y2: number;
    source: number;
    target: number;
  }> = [];

  for (const [, connsForPair] of groups) {
    const sorted = sortConnectionsForPair(connsForPair);
    sorted.forEach((conn, connIndex) => {
      if ((conn.medium ?? "WIRE") !== "WIRE") return;
      const seg = segmentForPairNthConnection(sorted, connIndex, getPosition, getFrameSize);
      if (!seg) return;
      wireSegments.push({
        ...seg,
        source: conn.source,
        target: conn.target,
      });
    });
  }

  // Only wires whose endpoints are being moved can change path; otherwise this would reject
  // unrelated moves (e.g. placing a new frame) whenever any existing WIRE crosses terrain.
  if (obstacleCells && obstacleCells.size > 0) {
    for (const seg of wireSegments) {
      if (!moves.has(seg.source) && !moves.has(seg.target)) continue;
      for (const key of obstacleCells) {
        const [ix, iy] = key.split(",").map(Number);
        if (
          segmentIntersectsAxisAlignedRect(
            seg.x1,
            seg.y1,
            seg.x2,
            seg.y2,
            ix * CELL,
            iy * CELL,
            CELL,
            CELL,
          )
        ) {
          return false;
        }
      }
    }
  }

  // WIRE vs third frame: only when something in this move can change the outcome — either the
  // wire's endpoints move (path updates) or this frame moves / is newly placed. Otherwise a
  // pre-existing wire-through-frame state would block every placement and unrelated drags.
  for (const f of frames) {
    const fid = f.id;
    const pos = getPosition(fid);
    if (!pos) continue;
    const cells = FRAME_CELL_SIZES[f.size] ?? 1;
    const fw = cells * CELL;

    for (const seg of wireSegments) {
      if (fid === seg.source || fid === seg.target) continue;
      if (!moves.has(fid) && !moves.has(seg.source) && !moves.has(seg.target)) continue;
      if (
        segmentIntersectsAxisAlignedRect(
          seg.x1,
          seg.y1,
          seg.x2,
          seg.y2,
          pos.x,
          pos.y,
          fw,
          fw,
        )
      ) {
        return false;
      }
    }
  }

  return true;
}

function groupMoveAvoidsObstacleCells(
  moves: Map<number, { x: number; y: number }>,
  frames: readonly { id: number; size: string; position?: { x: number; y: number } }[],
  obstacleCells: Set<string>,
): boolean {
  if (obstacleCells.size === 0) return true;
  for (const [id, pos] of moves) {
    const frame = frames.find((f) => f.id === id);
    if (!frame) continue;
    const keys = frameOccupiedCellKeys(pos, frame.size);
    for (const k of keys) {
      if (obstacleCells.has(k)) return false;
    }
  }
  return true;
}

export function groupMoveAvoidsFrameOverlap(
  moves: Map<number, { x: number; y: number }>,
  frames: readonly { id: number; size: string; position?: { x: number; y: number } }[],
  framePositions: ReadonlyMap<number, { x: number; y: number }>,
): boolean {
  for (const [id, pos] of moves) {
    const frame = frames.find((f) => f.id === id);
    if (!frame) continue;
    const cells = FRAME_CELL_SIZES[frame.size] ?? 1;
    const w = cells * CELL;
    const h = w;
    for (const f of frames) {
      if (f.id === id) continue;
      const other = moves.get(f.id);
      const ox = other ? other.x : framePositions.get(f.id)?.x ?? f.position?.x ?? 0;
      const oy = other ? other.y : framePositions.get(f.id)?.y ?? f.position?.y ?? 0;
      const fc = FRAME_CELL_SIZES[f.size] ?? 1;
      const fw = fc * CELL;
      const fh = fw;
      if (pos.x < ox + fw && pos.x + w > ox && pos.y < oy + fh && pos.y + h > oy) {
        return false;
      }
    }
  }
  return true;
}

/** Frame–frame overlap and WIRE intersection checks (same rules as drag). */
export function isGroupMoveValid(
  moves: Map<number, { x: number; y: number }>,
  connections: readonly ConnectionLike[],
  frames: readonly { id: number; size: string; position?: { x: number; y: number } }[],
  framePositions: ReadonlyMap<number, { x: number; y: number }>,
  obstacleCells?: Set<string>,
): boolean {
  if (obstacleCells && obstacleCells.size > 0 && !groupMoveAvoidsObstacleCells(moves, frames, obstacleCells)) {
    return false;
  }
  if (!groupMoveAvoidsFrameOverlap(moves, frames, framePositions)) return false;
  if (!groupMoveAvoidsWireIntersections(moves, connections, frames, framePositions, obstacleCells)) return false;
  return true;
}

/** Synthetic id for validating a frame that does not exist yet (negative — real ids are positive). */
export const HYPOTHETICAL_FRAME_PLACEHOLDER_ID = -0x7eed0001;

/**
 * Validate top-left placement for a frame not yet created (same rules as {@link isGroupMoveValid} / drag).
 */
export function isHypotheticalFramePlacementValid(
  topLeft: { x: number; y: number },
  sizeKey: string,
  existingFrames: readonly { id: number; size: string; position?: { x: number; y: number } }[],
  connections: readonly ConnectionLike[],
  framePositions: ReadonlyMap<number, { x: number; y: number }>,
  obstacleCells?: Set<string>,
): boolean {
  const frames = [
    ...existingFrames,
    {
      id: HYPOTHETICAL_FRAME_PLACEHOLDER_ID,
      size: sizeKey,
      position: topLeft,
    },
  ];
  const moves = new Map<number, { x: number; y: number }>([
    [HYPOTHETICAL_FRAME_PLACEHOLDER_ID, topLeft],
  ]);
  return isGroupMoveValid(moves, connections, frames, framePositions, obstacleCells);
}

/**
 * Control zone circle descriptor (from server).
 * `x`, `y` are the center of the zone source frame in world px.
 * `radius` is in grid cells.
 */
export interface ControlZoneCircle {
  x: number;
  y: number;
  radius: number;
}

/**
 * Returns true if the frame rectangle (top-left at `topLeft`, size `sizeKey`)
 * has **any** cell overlapping at least one control zone circle.
 * An empty zones array means no control zones exist (placement unrestricted).
 */
export function isFrameInControlZone(
  topLeft: { x: number; y: number },
  sizeKey: string,
  zones: readonly ControlZoneCircle[],
): boolean {
  if (zones.length === 0) return true; // no zones defined → unrestricted
  const cells = FRAME_CELL_SIZES[sizeKey] ?? 2;
  // Frame center in world px
  const half = (cells * CELL) / 2;
  const cx = topLeft.x + half;
  const cy = topLeft.y + half;
  for (const zone of zones) {
    const radiusPx = zone.radius * CELL;
    const dx = cx - zone.x;
    const dy = cy - zone.y;
    if (dx * dx + dy * dy <= radiusPx * radiusPx) {
      return true;
    }
  }
  return false;
}
