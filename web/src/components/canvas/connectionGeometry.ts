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

export const OFFSET_STEP = 6;

export interface ConnectionLike {
  source: number;
  target: number;
  type: string;
  medium?: string;
  transfer_progress?: number;
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

  const srcCells = FRAME_CELL_SIZES[srcSize] ?? 1;
  const tgtCells = FRAME_CELL_SIZES[tgtSize] ?? 1;
  const baseX1 = srcPos.x + (srcCells * CELL) / 2;
  const baseY1 = srcPos.y + (srcCells * CELL) / 2;
  const baseX2 = tgtPos.x + (tgtCells * CELL) / 2;
  const baseY2 = tgtPos.y + (tgtCells * CELL) / 2;

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

  const srcCells = FRAME_CELL_SIZES[srcSize] ?? 1;
  const tgtCells = FRAME_CELL_SIZES[tgtSize] ?? 1;
  const baseX1 = srcPos.x + (srcCells * CELL) / 2;
  const baseY1 = srcPos.y + (srcCells * CELL) / 2;
  const baseX2 = tgtPos.x + (tgtCells * CELL) / 2;
  const baseY2 = tgtPos.y + (tgtCells * CELL) / 2;

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
    for (const conn of connsForPair) {
      if ((conn.medium ?? "WIRE") !== "WIRE") continue;
      const seg = segmentForPairAndType(connsForPair, conn.type, getPosition, getFrameSize);
      if (!seg) continue;
      wireSegments.push({
        ...seg,
        source: conn.source,
        target: conn.target,
      });
    }
  }

  for (const frameId of moves.keys()) {
    const pos = getPosition(frameId);
    if (!pos) continue;
    const frame = frames.find((f) => f.id === frameId);
    const cells = FRAME_CELL_SIZES[frame?.size ?? "S"] ?? 1;
    const w = cells * CELL;
    const rect = { x: pos.x, y: pos.y, w, h: w };

    for (const seg of wireSegments) {
      if (frameId === seg.source || frameId === seg.target) continue;
      if (
        segmentIntersectsAxisAlignedRect(
          seg.x1,
          seg.y1,
          seg.x2,
          seg.y2,
          rect.x,
          rect.y,
          rect.w,
          rect.h,
        )
      ) {
        return false;
      }
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
): boolean {
  if (!groupMoveAvoidsFrameOverlap(moves, frames, framePositions)) return false;
  if (!groupMoveAvoidsWireIntersections(moves, connections, frames, framePositions)) return false;
  return true;
}
