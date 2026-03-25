import { describe, expect, it } from "vitest";
import {
  CELL,
  frameHasCoreForDataConnections,
  groupMoveAvoidsWireIntersections,
  isGroupMoveValid,
  isHypotheticalFramePlacementValid,
  obstacleCellKeysFromPatches,
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

  it("blocks moving a wire endpoint when the updated wire would cut a stationary third frame", () => {
    const wide = [
      { id: 1, size: "S", position: { x: 0, y: 0 } },
      { id: 2, size: "S", position: { x: 400, y: 0 } },
      { id: 3, size: "S", position: { x: 200, y: 0 } },
    ] as const;
    const posWide = new Map<number, { x: number; y: number }>([
      [1, { x: 0, y: 0 }],
      [2, { x: 400, y: 0 }],
      [3, { x: 200, y: 0 }],
    ]);
    const moves = new Map([[1, { x: 120, y: 0 }]]);
    const ok = groupMoveAvoidsWireIntersections(
      moves,
      [{ source: 1, target: 2, type: "POWER", medium: "WIRE" }],
      wide,
      posWide,
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

  it("blocks WIRE through obstacle cells", () => {
    const moves = new Map([[1, { x: 0, y: 0 }]]);
    // Horizontal POWER WIRE between (0,0) and (300,0) S-frames runs at y≈37.5 → grid row iy=1.
    const rocks = new Set(["6,1"]);
    const ok = groupMoveAvoidsWireIntersections(
      moves,
      [{ source: 1, target: 2, type: "POWER", medium: "WIRE" }],
      frames,
      positions,
      rocks,
    );
    expect(ok).toBe(false);
  });
});

describe("obstacleCellKeysFromPatches", () => {
  it("collects only obstacle patches", () => {
    const s = obstacleCellKeysFromPatches([
      { obstacle: true, cells: [[1, 2]] },
      { obstacle: false, cells: [[9, 9]] },
    ]);
    expect(s.has("1,2")).toBe(true);
    expect(s.has("9,9")).toBe(false);
  });
});

describe("isGroupMoveValid + obstacles", () => {
  const frames = [{ id: 1, size: "S", position: { x: 0, y: 0 } }] as const;
  const positions = new Map<number, { x: number; y: number }>([[1, { x: 0, y: 0 }]]);

  it("rejects frame top-left on rock cell", () => {
    const rocks = new Set(["0,0"]);
    const moves = new Map([[1, { x: 0, y: 0 }]]);
    expect(isGroupMoveValid(moves, [], [...frames], positions, rocks)).toBe(false);
  });
});

describe("isHypotheticalFramePlacementValid", () => {
  it("matches isGroupMoveValid for a new S frame", () => {
    const existing = [{ id: 1, size: "S", position: { x: 0, y: 0 } }];
    const positions = new Map<number, { x: number; y: number }>([[1, { x: 0, y: 0 }]]);
    expect(isHypotheticalFramePlacementValid({ x: 200, y: 0 }, "S", existing, [], positions)).toBe(true);
    expect(isHypotheticalFramePlacementValid({ x: 0, y: 0 }, "S", existing, [], positions)).toBe(false);
  });

  it("does not reject placement just because an existing WIRE crosses terrain (endpoints not moved)", () => {
    const existing = [
      { id: 1, size: "S", position: { x: 0, y: 0 } },
      { id: 2, size: "S", position: { x: 300, y: 0 } },
    ];
    const positions = new Map<number, { x: number; y: number }>([
      [1, { x: 0, y: 0 }],
      [2, { x: 300, y: 0 }],
    ]);
    const rocks = new Set(["6,1"]);
    const conns = [{ source: 1, target: 2, type: "POWER" as const, medium: "WIRE" }];
    expect(
      isHypotheticalFramePlacementValid({ x: 0, y: 300 }, "S", existing, conns, positions, rocks),
    ).toBe(true);
  });

  it("does not reject new frame placement because a WIRE already passes through a third frame (static violation)", () => {
    const existing = [
      { id: 1, size: "S", position: { x: 0, y: 0 } },
      { id: 2, size: "S", position: { x: 300, y: 0 } },
      // Sits on the WIRE segment between 1 and 2 (horizontal edge path).
      { id: 3, size: "S", position: { x: 150, y: 0 } },
    ];
    const positions = new Map<number, { x: number; y: number }>([
      [1, { x: 0, y: 0 }],
      [2, { x: 300, y: 0 }],
      [3, { x: 150, y: 0 }],
    ]);
    const conns = [{ source: 1, target: 2, type: "POWER" as const, medium: "WIRE" }];
    expect(
      isHypotheticalFramePlacementValid({ x: 0, y: 200 }, "S", existing, conns, positions),
    ).toBe(true);
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
    expect(power!.y1).toBeCloseTo(data!.y1 - 10, 5);
  });
});

describe("frameHasCoreForDataConnections", () => {
  it("is true for Main Core by name or logical type Core", () => {
    expect(frameHasCoreForDataConnections({ components: [{ name: "Main Core" }] })).toBe(true);
    expect(frameHasCoreForDataConnections({ components: [{ name: "X", type: "Core" }] })).toBe(true);
    expect(frameHasCoreForDataConnections({ components: [{ name: "Core" }] })).toBe(true);
  });

  it("is false without a core-like component", () => {
    expect(frameHasCoreForDataConnections({ components: [{ name: "Miner" }] })).toBe(false);
    expect(frameHasCoreForDataConnections({})).toBe(false);
  });
});
