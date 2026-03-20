import { describe, expect, it } from "vitest";
import {
  CELL,
  groupMoveAvoidsWireIntersections,
  segmentForPairAndType,
  segmentIntersectsAxisAlignedRect,
  snappedTopLeftsOverlappingCell,
} from "./connectionGeometry";

describe("segmentIntersectsAxisAlignedRect", () => {
  it("detects horizontal segment through rect interior", () => {
    expect(segmentIntersectsAxisAlignedRect(0, 0, 10, 0, 2, -1, 4, 2)).toBe(true);
  });

  it("returns false when segment is fully above rect", () => {
    expect(segmentIntersectsAxisAlignedRect(0, 5, 10, 5, 2, 0, 4, 2)).toBe(false);
  });
});

describe("groupMoveAvoidsWireIntersections", () => {
  const frames = [
    { id: 1, size: "S", position: { x: 0, y: 0 } },
    { id: 2, size: "S", position: { x: 300, y: 0 } },
    { id: 3, size: "S", position: { x: 150, y: 150 } },
  ] as const;

  const positions = new Map<number, { x: number; y: number }>([
    [1, { x: 0, y: 0 }],
    [2, { x: 300, y: 0 }],
    [3, { x: 150, y: 150 }],
  ]);

  it("allows moving endpoint along its own wire", () => {
    const moves = new Map([[1, { x: CELL, y: 0 }]]);
    const ok = groupMoveAvoidsWireIntersections(
      moves,
      [{ source: 1, target: 2, type: "POWER", medium: "WIRE" }],
      frames,
      positions,
    );
    expect(ok).toBe(true);
  });

  it("blocks moving a third frame through a WIRE segment", () => {
    const moves = new Map([[3, { x: 150, y: 0 }]]);
    const ok = groupMoveAvoidsWireIntersections(
      moves,
      [{ source: 1, target: 2, type: "POWER", medium: "WIRE" }],
      frames,
      positions,
    );
    expect(ok).toBe(false);
  });

  it("ignores WIRELESS for blocking", () => {
    const moves = new Map([[3, { x: 150, y: 0 }]]);
    const ok = groupMoveAvoidsWireIntersections(
      moves,
      [{ source: 1, target: 2, type: "POWER", medium: "WIRELESS" }],
      frames,
      positions,
    );
    expect(ok).toBe(true);
  });
});

describe("snappedTopLeftsOverlappingCell", () => {
  it("lists top-lefts whose frame overlaps a grid cell (S = 3×3 cells)", () => {
    const side = 3 * CELL;
    const tops = snappedTopLeftsOverlappingCell(0, 0, side);
    expect(tops.some((p) => p.x === 0 && p.y === 0)).toBe(true);
    expect(tops.some((p) => p.x === -2 * CELL && p.y === 0)).toBe(true);
  });
});

describe("segmentForPairAndType", () => {
  it("matches offset order when multiple types share a pair", () => {
    const conns = [
      { source: 1, target: 2, type: "POWER", medium: "WIRE" },
      { source: 1, target: 2, type: "DATA", medium: "WIRELESS" },
    ];
    const positions = new Map<number, { x: number; y: number }>([
      [1, { x: 0, y: 0 }],
      [2, { x: 300, y: 0 }],
    ]);
    const getPosition = (id: number) => positions.get(id) ?? null;
    const getFrameSize = () => "S";

    const power = segmentForPairAndType(conns, "POWER", getPosition, getFrameSize);
    const data = segmentForPairAndType(conns, "DATA", getPosition, getFrameSize);
    expect(power).not.toBeNull();
    expect(data).not.toBeNull();
    expect(power!.y1).toBeCloseTo(data!.y1 - 6, 5);
  });
});
