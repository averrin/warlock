import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { Application, Container, Graphics } from "pixi.js";
import type { FederatedPointerEvent } from "pixi.js";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { usePatchStore, Patch } from "../../stores/patches";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import type { ComponentDTO, ConnectionDTO } from "../../rpc/types";
import { ComponentContextMenu } from "./ComponentContextMenu";
import { PatchMiniInspector } from "./PatchMiniInspector";
import { ComponentPicker, BlueprintPicker } from "../picker";
import { FrameOverlay, type FrameOverlayHandle } from "./FrameOverlay";
import { useFrameDrag } from "./useFrameDrag";
import type { PlacedFrame } from "./FrameCard";
import { netAvailable } from "../../utils/power";
import {
  CELL,
  FRAME_CELL_SIZES,
  connectionWiringGeometryOk,
  edgeExitTowardPoint,
  frameCenterFromTopLeft,
  isGroupMoveValid,
  isHypotheticalFramePlacementValid,
  obstacleCellKeysFromPatches,
  OFFSET_STEP,
  segmentForPairNthConnection,
  snappedTopLeftsOverlappingCell,
  sortConnectionsForPair,
} from "./connectionGeometry";

type Props = {
  rpcClient: RpcClient;
  onFrameMiniInspect?: (
    frameId: number,
    clientX: number,
    clientY: number,
    opts?: { keepInPlace: boolean },
  ) => void;
};

const MIN_ZOOM = 0.15;
const MAX_ZOOM = 3.0;
const ZOOM_FACTOR = 0.1;
const GRID_LINE_COLOR = 0x1f2937;
const GRID_LINE_ALPHA = 0.35;
const GRID_DOT_COLOR = 0x374151;
const GRID_DOT_ALPHA = 0.5;
const GRID_DOT_RADIUS = 1.5;
/** Hide grid when one cell projects smaller than this (screen px). */
const GRID_HIDE_THRESHOLD = 4;
/** Dots at corners every N cells (S frame = 3 cells per side). */
const GRID_DOT_STRIDE_CELLS = 3;

const CONN_COLOR: Record<string, number> = {
  POWER: 0xeab308,
  DATA: 0x3b82f6,
  CONVEYOR: 0x22c55e,
};

const POWER_LINE_DIM = 0x6b7280;
const CONVEYOR_DIM = 0x166534;
const DASH_LEN = 7;
const DASH_GAP = 5;

function strokeDashedLine(
  g: Graphics,
  x1: number,
  y1: number,
  x2: number,
  y2: number,
  dash: number,
  gap: number,
  width: number,
  color: number,
  alpha: number,
  phase: number,
) {
  const dx = x2 - x1;
  const dy = y2 - y1;
  const len = Math.hypot(dx, dy);
  if (len < 1e-6) return;
  const ux = dx / len;
  const uy = dy / len;
  const period = dash + gap;
  let t = phase % period;
  if (t < 0) t += period;
  t = -t;
  while (t < len) {
    const t0 = Math.max(0, t);
    const t1 = Math.min(len, t + dash);
    if (t0 + 1e-4 < t1) {
      g.setStrokeStyle({ width, color, alpha });
      g.moveTo(x1 + ux * t0, y1 + uy * t0);
      g.lineTo(x1 + ux * t1, y1 + uy * t1);
      g.stroke();
    }
    t += period;
  }
}

const DEFAULT_MARKER_COLOR = 0xff0000;

/** Parse #RGB / #RRGGBB / #RRGGBBAA (alpha ignored for Pixi fill). */
function parseHexColorString(s: string): number | null {
  const t = s.trim().replace(/^#/, "");
  if (!t) return null;
  if (/^[0-9a-fA-F]{3}$/.test(t)) {
    return parseInt(
      t
        .split("")
        .map((c) => c + c)
        .join(""),
      16,
    );
  }
  if (/^[0-9a-fA-F]{6}$/.test(t)) {
    return parseInt(t, 16);
  }
  if (/^[0-9a-fA-F]{8}$/.test(t)) {
    return parseInt(t.slice(0, 6), 16);
  }
  return null;
}

/** Map marker color from server/Lua (hex, CSS names, rgb()) to Pixi 0xRRGGBB; unknown strings → default. */
function markerColorToPixi(color: string | undefined): number {
  if (color == null) return DEFAULT_MARKER_COLOR;
  const trimmed = color.trim();
  if (!trimmed) return DEFAULT_MARKER_COLOR;

  const hex = parseHexColorString(trimmed);
  if (hex != null) return hex;

  if (typeof document === "undefined") return DEFAULT_MARKER_COLOR;

  const probe = document.createElement("div");
  probe.style.color = "";
  probe.style.color = trimmed;
  if (!probe.style.color) return DEFAULT_MARKER_COLOR;

  const ctx = document.createElement("canvas").getContext("2d");
  if (!ctx) return DEFAULT_MARKER_COLOR;
  ctx.fillStyle = trimmed;
  const normalized = ctx.fillStyle as string;
  if (normalized.startsWith("#")) {
    const h = normalized.slice(1);
    if (h.length === 3) {
      return parseInt(
        h
          .split("")
          .map((c) => c + c)
          .join(""),
        16,
      );
    }
    return parseInt(h.length >= 6 ? h.slice(0, 6) : h, 16) & 0xffffff;
  }
  const rgb = /^rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)/.exec(normalized);
  if (rgb) {
    return (
      ((Number(rgb[1]) & 255) << 16) |
      ((Number(rgb[2]) & 255) << 8) |
      (Number(rgb[3]) & 255)
    );
  }
  return DEFAULT_MARKER_COLOR;
}

const CONNECTOR_COMPONENT: Record<string, string> = {
  POWER: "Power Wire Connector",
  DATA: "Data Wire Connector",
  CONVEYOR: "Conveyor Connector",
};

type ConnectionMedium = "WIRE" | "WIRELESS" | "BEAM";

function getConnectorComponentName(type: string, medium: ConnectionMedium): string | null {
  if (type === "POWER") {
    if (medium === "WIRE") return "Power Wire Connector";
    if (medium === "WIRELESS") return "Power Wireless Connector";
    return "Power Beam Connector";
  }
  if (type === "DATA") {
    if (medium === "WIRE") return "Data Wire Connector";
    if (medium === "WIRELESS") return "Wireless Data Connector";
    return "Data Beam Connector";
  }
  if (type === "CONVEYOR") {
    return "Conveyor Connector";
  }
  if (type === "POE") {
    return "PoE Connector";
  }
  return null;
}

/** Primary connector + relay variants that satisfy the same connection role. */
function getConnectorNamesForFrame(type: string, medium: ConnectionMedium): string[] {
  const primary = getConnectorComponentName(type, medium);
  if (!primary) return [];
  const names = [primary];
  if (type === "DATA" && medium === "WIRE") names.push("Data Relay");
  if (type === "CONVEYOR") names.push("Conveyor Relay");
  return names;
}

function toCssHex(n: number): string {
  return `#${n.toString(16).padStart(6, "0")}`;
}

function frameHasComponentByName(
  frame: { components?: { name: string }[] },
  componentName: string,
): boolean {
  return (frame.components ?? []).some((c) => c.name === componentName);
}

function frameHasConnector(
  frame: { components?: { name: string }[] },
  type: string,
  medium: ConnectionMedium,
): boolean {
  const names = getConnectorNamesForFrame(type, medium);
  return names.some((n) => frameHasComponentByName(frame, n));
}

function frameMeetsConnectionRequirements(
  frame: { components?: ComponentDTO[] },
  type: string,
  medium: ConnectionMedium,
): boolean {
  if (type === "CONVEYOR") {
    if ((frame.components ?? []).some((c) => c.name === "Conveyor Relay")) return true;
    return (frame.components ?? []).some((c) => c.storage);
  }
  if (type === "DATA" || type === "POE") {
    if ((frame.components ?? []).some((c) => c.name === "Data Relay")) return true;
    return frameHasComponentByName(frame, "Core");
  }
  return true;
}

function isConveyorPairMisconfigured(
  sourceFrame: { components?: ComponentDTO[] } | undefined,
  targetFrame: { components?: ComponentDTO[] } | undefined,
): boolean {
  if (!sourceFrame || !targetFrame) return true;
  return (
    !frameMeetsConnectionRequirements(sourceFrame, "CONVEYOR", "WIRE") ||
    !frameMeetsConnectionRequirements(targetFrame, "CONVEYOR", "WIRE")
  );
}

function getMaxConnectionsForFrameType(
  frames: { id: number; components?: ComponentDTO[] }[],
  frameId: number,
  type: string,
  medium: ConnectionMedium,
): number {
  const frame = frames.find((f) => f.id === frameId);
  if (!frame) return type === "POWER" ? 10 : 1;
  const nameSet = new Set(getConnectorNamesForFrame(type, medium));
  const def = type === "POWER" ? 10 : 1;
  let maxTotal = 0;
  for (const c of frame.components ?? []) {
    if (!nameSet.has(c.name)) continue;
    const attr =
      c.metadata?.attributes?.max_connections ??
      (c.attributes as Record<string, unknown> | undefined)?.max_connections;
    const n = parseFinalNumber(attr as any);
    maxTotal += n != null ? n : def;
  }
  if (maxTotal <= 0) return def;
  return maxTotal;
}

function getConnectionCountForFrameType(
  conns: { source: number; target: number; type: string; medium?: string }[],
  frameId: number,
  type: string,
  medium: ConnectionMedium,
): number {
  return conns.filter((c) => {
    const cm = c.medium ?? "WIRE";
    return (c.source === frameId || c.target === frameId) && c.type === type && cm === medium;
  }).length;
}

function isCardinallySatisfied(
  conns: { source: number; target: number; type: string; medium?: string }[],
  frames: { id: number; components?: ComponentDTO[] }[],
  frameId: number,
  type: string,
  medium: ConnectionMedium,
): boolean {
  const max = getMaxConnectionsForFrameType(frames, frameId, type, medium);
  if (max <= 0) return false;
  const count = getConnectionCountForFrameType(conns, frameId, type, medium);
  return count < max;
}

function hasConnectionBetween(
  conns: { source: number; target: number; type: string; medium?: string }[],
  a: number,
  b: number,
  type: string,
  medium: ConnectionMedium,
): boolean {
  return conns.some(
    (c) =>
      ((c.source === a && c.target === b) || (c.source === b && c.target === a)) &&
      c.type === type &&
      (c.medium ?? "WIRE") === medium,
  );
}

function parseFinalNumber(v: any): number | null {
  if (typeof v === "number") return v;
  if (v && typeof v === "object") {
    if (typeof v.final_value !== "undefined") {
      const n = typeof v.final_value === "number" ? v.final_value : Number(v.final_value);
      if (!Number.isNaN(n)) return n;
    }
    if (typeof v.base_value !== "undefined") {
      const n = typeof v.base_value === "number" ? v.base_value : Number(v.base_value);
      if (!Number.isNaN(n)) return n;
    }
  }
  if (typeof v !== "undefined") {
    const n = Number(v);
    if (!Number.isNaN(n)) return n;
  }
  return null;
}

function getFrameSquareSide(frame: { size: string }): number {
  const cells = FRAME_CELL_SIZES[frame.size] ?? 1;
  return cells * CELL;
}

function getFrameCenter(frame: { position?: { x: number; y: number }; size: string }): { x: number; y: number } | null {
  if (!frame.position) return null;
  const side = getFrameSquareSide(frame);
  return { x: frame.position.x + side / 2, y: frame.position.y + side / 2 };
}

function getMaxConnectionDistanceForFrame(
  frame: { components?: ComponentDTO[]; size: string; id: number },
  type: string,
  medium: ConnectionMedium,
): number {
  const names = getConnectorNamesForFrame(type, medium);
  let best = Number.POSITIVE_INFINITY;
  for (const n of names) {
    const connector = (frame.components ?? []).find((c) => c.name === n);
    if (!connector) continue;
    const attr =
      connector.metadata?.attributes?.max_connection_distance ??
      (connector.attributes as Record<string, unknown> | undefined)?.max_connection_distance;
    const d = parseFinalNumber(attr);
    if (d != null) best = Math.min(best, d);
  }
  return best;
}

function segmentIntersectsAABB(
  ax: number,
  ay: number,
  bx: number,
  by: number,
  rx: number,
  ry: number,
  rw: number,
  rh: number,
): boolean {
  const minX = rx;
  const maxX = rx + rw;
  const minY = ry;
  const maxY = ry + rh;

  const dx = bx - ax;
  const dy = by - ay;

  let tmin = 0;
  let tmax = 1;
  const eps = 1e-6;

  if (Math.abs(dx) < eps) {
    if (ax < minX || ax > maxX) return false;
  } else {
    const inv = 1 / dx;
    let tx1 = (minX - ax) * inv;
    let tx2 = (maxX - ax) * inv;
    if (tx1 > tx2) [tx1, tx2] = [tx2, tx1];
    tmin = Math.max(tmin, tx1);
    tmax = Math.min(tmax, tx2);
    if (tmin > tmax) return false;
  }

  if (Math.abs(dy) < eps) {
    if (ay < minY || ay > maxY) return false;
  } else {
    const inv = 1 / dy;
    let ty1 = (minY - ay) * inv;
    let ty2 = (maxY - ay) * inv;
    if (ty1 > ty2) [ty1, ty2] = [ty2, ty1];
    tmin = Math.max(tmin, ty1);
    tmax = Math.min(tmax, ty2);
    if (tmin > tmax) return false;
  }

  return true;
}

function wiringGeometryOk(
  sourceId: number,
  targetId: number,
  wType: "POWER" | "DATA" | "CONVEYOR" | "POE",
  wMedium: ConnectionMedium,
  currentFrames: { id: number; size: string; position?: { x: number; y: number }; components?: ComponentDTO[] }[],
  currentConns: ConnectionDTO[],
  getPosition: (id: number) => { x: number; y: number } | null,
  getFrameSize: (id: number) => string | undefined,
  obstacleCells?: Set<string>,
): boolean {
  const sourceFrame = currentFrames.find((f) => f.id === sourceId);
  const tgtFrame = currentFrames.find((f) => f.id === targetId);
  const srcOrigin = getPosition(sourceId);
  const tgtOrigin = getPosition(targetId);
  if (!sourceFrame || !tgtFrame || !srcOrigin || !tgtOrigin) return false;
  const sourceCenter = frameCenterFromTopLeft(srcOrigin, getFrameSize(sourceId));
  const targetCenter = frameCenterFromTopLeft(tgtOrigin, getFrameSize(targetId));
  const centerDist = Math.hypot(targetCenter.x - sourceCenter.x, targetCenter.y - sourceCenter.y);
  const sourceMax = getMaxConnectionDistanceForFrame(sourceFrame, wType, wMedium);
  const targetMax = getMaxConnectionDistanceForFrame(tgtFrame, wType, wMedium);
  const effectiveMax = Math.min(sourceMax, targetMax);
  return connectionWiringGeometryOk(
    sourceId,
    targetId,
    wType,
    wMedium,
    effectiveMax,
    centerDist,
    currentFrames,
    currentConns,
    getPosition,
    getFrameSize,
    obstacleCells,
  );
}

function redrawGrid(
  grid: Graphics,
  world: Container,
  screenW: number,
  screenH: number,
) {
  grid.clear();

  const scale = world.scale.x;
  const wx0 = -world.x / scale;
  const wy0 = -world.y / scale;
  const wx1 = wx0 + screenW / scale;
  const wy1 = wy0 + screenH / scale;

  const cellPx = CELL * scale;
  if (cellPx < GRID_HIDE_THRESHOLD) return;

  const sxMin = Math.floor(wx0 / CELL) * CELL;
  const sxMax = Math.ceil(wx1 / CELL) * CELL;
  const syMin = Math.floor(wy0 / CELL) * CELL;
  const syMax = Math.ceil(wy1 / CELL) * CELL;

  grid.setStrokeStyle({ width: 1, color: GRID_LINE_COLOR, alpha: GRID_LINE_ALPHA });
  for (let x = sxMin; x <= sxMax; x += CELL) {
    grid.moveTo(x, syMin);
    grid.lineTo(x, syMax);
  }
  for (let y = syMin; y <= syMax; y += CELL) {
    grid.moveTo(sxMin, y);
    grid.lineTo(sxMax, y);
  }
  grid.stroke();

  const dotStep = GRID_DOT_STRIDE_CELLS * CELL;
  const dotMinX = Math.floor(wx0 / dotStep) * dotStep;
  const dotMaxX = Math.ceil(wx1 / dotStep) * dotStep;
  const dotMinY = Math.floor(wy0 / dotStep) * dotStep;
  const dotMaxY = Math.ceil(wy1 / dotStep) * dotStep;
  for (let x = dotMinX; x <= dotMaxX; x += dotStep) {
    for (let y = dotMinY; y <= dotMaxY; y += dotStep) {
      grid.circle(x, y, GRID_DOT_RADIUS);
    }
  }
  grid.fill({ color: GRID_DOT_COLOR, alpha: GRID_DOT_ALPHA });
}

export function GameCanvas({ rpcClient, onFrameMiniInspect }: Props) {
  const hostRef = useRef<HTMLDivElement | null>(null);
  const appRef = useRef<Application | null>(null);
  const worldRef = useRef<Container | null>(null);
  const gridRef = useRef<Graphics | null>(null);
  const patchLayerRef = useRef<Container | null>(null);
  const overlayRef = useRef<FrameOverlayHandle | null>(null);
  const patches = usePatchStore((s) => s.patches);
  const patchesRef = useRef(patches);
  patchesRef.current = patches;
  const obstacleCells = useMemo(() => obstacleCellKeysFromPatches(patches), [patches]);
  const obstacleCellsRef = useRef(obstacleCells);
  obstacleCellsRef.current = obstacleCells;
  const patchTypes = usePatchStore((s) => s.patchTypes);
  const frames = useGameStore((s) => s.frames);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
  const selectedFrameIds = useGameStore((s) => s.selectedFrameIds);
  const setFrameSelection = useGameStore((s) => s.setFrameSelection);
  const toggleFrameSelection = useGameStore((s) => s.toggleFrameSelection);
  const markers = useGameStore((s) => s.markers);
  const markerLayerRef = useRef<Graphics | null>(null);
  const markersRef = useRef(markers);
  markersRef.current = markers;
  const selectFrame = useGameStore((s) => s.selectFrame);
  const moveFrame = useGameStore((s) => s.moveFrame);
  const createFromBlueprint = useGameStore((s) => s.createFromBlueprint);
  const updateFrameMetadata = useGameStore((s) => s.updateFrameMetadata);
  const panningRef = useRef(false);
  const panStartRef = useRef({ sx: 0, sy: 0, wx: 0, wy: 0 });
  const framesRef = useRef(frames);
  framesRef.current = frames;
  const ghostRef = useRef<Graphics | null>(null);
  const creatingRef = useRef<string | null>(null);
  const [zoom, setZoom] = useState(1);

  const [contextMenu, setContextMenu] = useState<{ x: number; y: number; wx: number; wy: number } | null>(null);
  const [creationBlueprint, setCreationBlueprint] = useState<string | null>(null);
  const [blueprintMap, setBlueprintMap] = useState<Record<string, string>>({});
  const blueprintMapRef = useRef<Record<string, string>>({});
  const blueprintSupported = isFeatureSupported(capabilityMethods.blueprintPalette);
  const connections = useGameStore((s) => s.connections);
  const powerNetworks = useGameStore((s) => s.powerNetworks);
  const createConnection = useGameStore((s) => s.createConnection);
  const removeConnection = useGameStore((s) => s.removeConnection);
  const removeFrame = useGameStore((s) => s.removeFrame);
  const connectionLayerRef = useRef<Graphics | null>(null);
  const wiringLineRef = useRef<Graphics | null>(null);
  const connectionsRef = useRef(connections);
  connectionsRef.current = connections;
  const powerNetworksRef = useRef(powerNetworks);
  powerNetworksRef.current = powerNetworks;
  const placedFramesRef = useRef<PlacedFrame[]>([]);
  const wiringRef = useRef<
    { sourceId: number; type: "POWER" | "DATA" | "CONVEYOR" | "POE"; medium: "WIRE" | "WIRELESS" | "BEAM" } | null
  >(null);
  const rpcClientRef = useRef(rpcClient);
  rpcClientRef.current = rpcClient;
  const selectedFrameIdsRef = useRef(selectedFrameIds);
  selectedFrameIdsRef.current = selectedFrameIds;
  const createConnectionRef = useRef(createConnection);
  createConnectionRef.current = createConnection;
  const [wiringMode, setWiringMode] = useState<
    { sourceId: number; type: "POWER" | "DATA" | "CONVEYOR" | "POE"; medium: "WIRE" | "WIRELESS" | "BEAM" } | null
  >(null);
  const [frameContextMenu, setFrameContextMenu] = useState<{ x: number; y: number; frameId: number } | null>(null);
  const [bulkFrameContextMenu, setBulkFrameContextMenu] = useState<{
    x: number;
    y: number;
    frameIds: number[];
  } | null>(null);
  const [frameRenaming, setFrameRenaming] = useState<{ frameId: number; name: string } | null>(null);
  const [compContextMenu, setCompContextMenu] = useState<{
    x: number; y: number; frameId: number; componentId: number;
  } | null>(null);
  const [componentPickerOpen, setComponentPickerOpen] = useState<{ frameId: number } | null>(null);
  const [blueprintPickerOpen, setBlueprintPickerOpen] = useState<{ wx: number; wy: number } | null>(null);
  const [patchInspector, setPatchInspector] = useState<{ patch: Patch; x: number; y: number } | null>(null);
  const drawConnectionsRef = useRef<() => void>(() => {});
  const drawRestrictedDropZonesRef = useRef<() => void>(() => {});
  const restrictedDropLayerRef = useRef<Graphics | null>(null);
  const selectionBoxRef = useRef<Graphics | null>(null);
  const boxSelectSessionRef = useRef<{
    active: boolean;
    wx0: number;
    wy0: number;
  } | null>(null);

  // --- Frame positions map for connection drawing and drag ---
  const framePositionsRef = useRef<Map<number, { x: number; y: number }>>(new Map());

  const getPatchAtWorldPos = (wx: number, wy: number): Patch | null => {
    const gridX = Math.floor(wx / CELL);
    const gridY = Math.floor(wy / CELL);
    const currentPatches = patchesRef.current;

    for (const patch of currentPatches) {
      if (patch.cells.some(([cx, cy]) => cx === gridX && cy === gridY)) {
        return patch;
      }
    }
    return null;
  };

  // Click-outside handler for all context menus
  useEffect(() => {
    const handleClickOutside = (e: MouseEvent) => {
      const target = e.target as HTMLElement;
      if (!target.closest("[data-context-menu]")) {
        setContextMenu(null);
        setFrameContextMenu(null);
        setBulkFrameContextMenu(null);
        setCompContextMenu(null);
        setFrameRenaming(null);
        setPatchInspector(null);
      }
    };
    document.addEventListener("mousedown", handleClickOutside);
    return () => document.removeEventListener("mousedown", handleClickOutside);
  }, []);

  // --- Connection drawing (reads from framePositionsRef) ---
  const drawConnections = () => {
    const connLayer = connectionLayerRef.current;
    if (!connLayer) return;
    connLayer.clear();

    const groups = new Map<string, typeof connectionsRef.current>();
    for (const conn of connectionsRef.current) {
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

    const dashAnimPhase = performance.now() * 0.004;

    for (const [, conns] of groups) {
      const sample = conns[0]!;
      const srcPos = framePositionsRef.current.get(sample.source);
      const tgtPos = framePositionsRef.current.get(sample.target);
      const srcFrame = framesRef.current.find((f) => f.id === sample.source);
      const tgtFrame = framesRef.current.find((f) => f.id === sample.target);
      if (!srcPos || !tgtPos || !srcFrame || !tgtFrame) continue;

      const getPosition = (id: number) => framePositionsRef.current.get(id) ?? null;
      const getFrameSize = (id: number) => framesRef.current.find((f) => f.id === id)?.size;

      const sorted = sortConnectionsForPair(conns) as ConnectionDTO[];
      sorted.forEach((conn, idx) => {
        const seg = segmentForPairNthConnection(sorted, idx, getPosition, getFrameSize);
        if (!seg) return;
        const { x1, y1, x2, y2 } = seg;

        const type = conn.type;
        const medium = (conn.medium ?? "WIRE") as ConnectionMedium;
        const conveyorBad = type === "CONVEYOR" && isConveyorPairMisconfigured(srcFrame, tgtFrame);

        let color = CONN_COLOR[type] ?? 0xffffff;
        let alpha = 0.7;
        let width = 2;

        if (type === "POWER") {
          const net = powerNetworksRef.current.find((n) => n.frames?.includes(conn.source));
          const noPower = !net || netAvailable(net) <= 0;
          if (noPower) {
            color = POWER_LINE_DIM;
            alpha = 0.55;
          }
        }

        if (conveyorBad) {
          color = CONVEYOR_DIM;
          alpha = 0.45;
        }

        const wirelessDash = medium === "WIRELESS";
        const conveyorAnim =
          type === "CONVEYOR" &&
          typeof conn.transfer_progress === "number" &&
          conn.transfer_progress > 0.02 &&
          conn.transfer_progress < 0.98;

        if (wirelessDash || conveyorAnim) {
          strokeDashedLine(
            connLayer,
            x1,
            y1,
            x2,
            y2,
            wirelessDash ? DASH_LEN : 5,
            wirelessDash ? DASH_GAP : 4,
            width,
            color,
            alpha,
            dashAnimPhase,
          );
        } else {
          connLayer.setStrokeStyle({ width, color, alpha });
          connLayer.moveTo(x1, y1);
          connLayer.lineTo(x2, y2);
          connLayer.stroke();
        }

        const mx = (x1 + x2) / 2;
        const my = (y1 + y2) / 2;
        const bundleCount = sorted.length;
        const sockR = bundleCount > 1 ? Math.min(3.5, OFFSET_STEP / 2 - 1) : 4;

        if (conveyorBad) {
          const arm = 5;
          connLayer.setStrokeStyle({ width: 2, color, alpha: alpha + 0.15 });
          connLayer.moveTo(mx - arm, my - arm);
          connLayer.lineTo(mx + arm, my + arm);
          connLayer.stroke();
          connLayer.moveTo(mx - arm, my + arm);
          connLayer.lineTo(mx + arm, my - arm);
          connLayer.stroke();
        } else {
          connLayer.circle(x1, y1, sockR);
          connLayer.fill({ color, alpha: 0.88 });
          connLayer.stroke({ color: 0xffffff, width: 1, alpha: 0.35 });
          connLayer.circle(x2, y2, sockR);
          connLayer.fill({ color, alpha: 0.88 });
          connLayer.stroke({ color: 0xffffff, width: 1, alpha: 0.35 });
        }
      });
    }
    drawRestrictedDropZonesRef.current();
  };
  drawConnectionsRef.current = drawConnections;

  const updateMarkers = useCallback(() => {
    const layer = markerLayerRef.current;
    if (!layer) return;
    layer.clear();
    const currentMarkers = markersRef.current;
    for (const marker of currentMarkers) {
      const colorNum = markerColorToPixi(marker.color);
      layer.circle(marker.x, marker.y, 8);
      layer.fill({ color: colorNum });
      layer.stroke({ color: 0xffffff, width: 2 });
    }
  }, []);

  useEffect(() => {
    updateMarkers();
  }, [markers, updateMarkers]);

  const renderPatches = useCallback(() => {
    const layer = patchLayerRef.current;
    if (!layer) return;
    layer.removeChildren();

    for (const patch of patches) {
      const gfx = new Graphics();
      const { r, g, b, a } = patch.color;
      const color = (r << 16) | (g << 8) | b;
      const alpha = a / 255;

      for (const [cx, cy] of patch.cells) {
        gfx.rect(cx * CELL, cy * CELL, CELL, CELL);
      }
      gfx.fill({ color, alpha });

      layer.addChild(gfx);
    }
  }, [patches]);

  useEffect(() => {
    renderPatches();
  }, [renderPatches]);

  const parseBlueprintSize = (source: string): string => {
    const match = source.match(/FrameSize\.(\w+)/);
    return match?.[1] ?? "M";
  };

  useEffect(() => {
    if (!blueprintSupported) return;
    void (async () => {
      try {
        const result = await rpcClient.call<{ blueprints: Record<string, string> }>("code.blueprints");
        const bp = result.blueprints ?? {};
        setBlueprintMap(bp);
        blueprintMapRef.current = bp;
      } catch {
        // Blueprints not available
      }
    })();
  }, [rpcClient, blueprintSupported]);

  const enterCreationMode = (blueprint: string) => {
    creatingRef.current = blueprint;
    setCreationBlueprint(blueprint);
  };

  const exitCreationMode = () => {
    creatingRef.current = null;
    setCreationBlueprint(null);
    if (ghostRef.current) {
      ghostRef.current.visible = false;
    }
  };

  const createPatch = async (patchType: string, wx: number, wy: number) => {
    const gridX = Math.floor(wx / CELL);
    const gridY = Math.floor(wy / CELL);
    try {
      await rpcClient.call("patches.create", {
        type: patchType,
        x: gridX,
        y: gridY,
      });
      const data = await rpcClient.call<{ patches: Patch[] }>("patches.list");
      usePatchStore.getState().setPatches(data.patches ?? []);
    } catch (e) {
      console.error("Failed to create patch:", e);
    }
    setContextMenu(null);
  };

  const placedFrames: PlacedFrame[] = useMemo(() => {
    return frames.map((frame, index) => {
      const slotStride = (FRAME_CELL_SIZES.S ?? 3) * CELL;
      const x = frame.position?.x ?? (index % 6) * slotStride + CELL;
      const y = frame.position?.y ?? Math.floor(index / 6) * slotStride + CELL;
      return { ...frame, _x: x, _y: y };
    });
  }, [frames]);
  placedFramesRef.current = placedFrames;

  // framePositionsRef sync is done after dragState is defined (see below)

  const requestGridRedraw = () => {
    const app = appRef.current;
    const world = worldRef.current;
    const grid = gridRef.current;
    if (!app || !world || !grid) return;
    redrawGrid(grid, world, app.screen.width, app.screen.height);
  };

  const isOverlapping = (x: number, y: number, size: string, excludeId: number | null) => {
    const cells = FRAME_CELL_SIZES[size] ?? 1;
    const w = cells * CELL;
    const h = w;
    for (const f of framesRef.current) {
      if (f.id === excludeId) continue;
      const fx = f.position?.x ?? 0;
      const fy = f.position?.y ?? 0;
      const fc = FRAME_CELL_SIZES[f.size] ?? 1;
      const fw = fc * CELL;
      const fh = fw;
      if (x < fx + fw && x + w > fx && y < fy + fh && y + h > fy) {
        return true;
      }
    }
    return false;
  };

  // --- Frame drag hook ---
  const dragState = useFrameDrag({
    hostRef,
    overlayRef,
    framesRef,
    framePositionsRef,
    obstacleCellsRef,
    drawConnections,
    connectionsRef,
    moveFrame,
    selectFrame,
    toggleFrameSelection,
    onFrameMiniInspect,
  });
  dragState.rpcClientRef.current = rpcClient;

  drawRestrictedDropZonesRef.current = () => {
    const layer = restrictedDropLayerRef.current;
    const world = worldRef.current;
    const app = appRef.current;
    if (!layer || !world || !app) return;
    layer.clear();
    const dragging = dragState.draggingFrameIdsRef.current;
    if (!dragging || dragging.size !== 1) return;

    const frameId = [...dragging][0]!;
    const dragFrame = framesRef.current.find((f) => f.id === frameId);
    if (!dragFrame) return;

    const fc = FRAME_CELL_SIZES[dragFrame.size] ?? 1;
    const side = fc * CELL;

    const scale = world.scale.x;
    const wx0 = -world.x / scale;
    const wy0 = -world.y / scale;
    const wx1 = wx0 + app.screen.width / scale;
    const wy1 = wy0 + app.screen.height / scale;

    const pad = CELL;
    const focusMinX = wx0 - pad;
    const focusMaxX = wx1 + pad;
    const focusMinY = wy0 - pad;
    const focusMaxY = wy1 + pad;

    const ixMin = Math.floor(focusMinX / CELL);
    const ixMax = Math.ceil(focusMaxX / CELL);
    const iyMin = Math.floor(focusMinY / CELL);
    const iyMax = Math.ceil(focusMaxY / CELL);

    let cellStride = 1;
    let approxCells = (ixMax - ixMin) * (iyMax - iyMin);
    while (approxCells > 14000 && cellStride < 5) {
      cellStride++;
      approxCells = Math.ceil((ixMax - ixMin) / cellStride) * Math.ceil((iyMax - iyMin) / cellStride);
    }

    // Per grid cell: red iff no snapped top-left exists that both overlaps this cell and is valid.
    // (Avoids misleading "top-left only" red that still allows the frame body to sit on wires.)
    for (let ix = ixMin; ix < ixMax; ix += cellStride) {
      for (let iy = iyMin; iy < iyMax; iy += cellStride) {
        const tops = snappedTopLeftsOverlappingCell(ix, iy, side);
        if (tops.length === 0) continue;

        let anyValid = false;
        for (const p of tops) {
          const candidate = new Map([[frameId, p]]);
          if (
            isGroupMoveValid(
              candidate,
              connectionsRef.current,
              framesRef.current,
              framePositionsRef.current,
              obstacleCellsRef.current,
            )
          ) {
            anyValid = true;
            break;
          }
        }
        if (!anyValid) {
          layer.rect(ix * CELL, iy * CELL, CELL * cellStride, CELL * cellStride);
        }
      }
    }
    layer.fill({ color: 0xff3355, alpha: 0.14 });
  };

  // Keep framePositionsRef in sync with placedFrames
  // Skip frames that are currently being dragged or have pending move RPCs
  useLayoutEffect(() => {
    for (const frame of placedFrames) {
      const isDragging = dragState.draggingFrameIdsRef.current?.has(frame.id) ?? false;
      const isPending = dragState.pendingMoveIds.current.has(frame.id);
      if (!isDragging && !isPending) {
        framePositionsRef.current.set(frame.id, { x: frame._x, y: frame._y });
      }
    }
    const activeIds = new Set(placedFrames.map((f) => f.id));
    for (const id of framePositionsRef.current.keys()) {
      if (!activeIds.has(id)) {
        framePositionsRef.current.delete(id);
      }
    }
    // dragState.* refs are stable; omit dragState object from deps (new wrapper each render).
  }, [placedFrames]); // eslint-disable-line react-hooks/exhaustive-deps

  const getEventClientXY = (e: FederatedPointerEvent): { x: number; y: number } => {
    const client = (e as any).client as { x?: number; y?: number } | undefined;
    if (typeof client?.x === "number" && typeof client?.y === "number") {
      return { x: client.x, y: client.y };
    }
    const oe = ((e as any).originalEvent ?? (e as any).nativeEvent ?? (e as any).data?.originalEvent) as
      | { clientX?: number; clientY?: number }
      | undefined;
    return { x: oe?.clientX ?? 0, y: oe?.clientY ?? 0 };
  };

  // --- Handle frame pointer down: wiring mode or delegate to drag ---
  const handleFramePointerDown = useCallback(
    (e: React.PointerEvent, frameId: number) => {
      // If wiring mode active, handle wiring target selection
      const wiring = wiringRef.current;
      if (wiring) {
        e.stopPropagation();
        const currentFrames = framesRef.current;
        const currentConns = connectionsRef.current;
        const tgtFrame = currentFrames.find((f) => f.id === frameId);
        const hasConn = tgtFrame ? frameHasConnector(tgtFrame, wiring.type, wiring.medium) : false;
        const hasReq = tgtFrame
          ? frameMeetsConnectionRequirements(tgtFrame, wiring.type, wiring.medium)
          : false;
        const cardOk = isCardinallySatisfied(currentConns, currentFrames, frameId, wiring.type, wiring.medium);
        const pairOk = !hasConnectionBetween(currentConns, wiring.sourceId, frameId, wiring.type, wiring.medium);
        const sourceFrame = currentFrames.find((f) => f.id === wiring.sourceId);
        const getPosition = (id: number) => {
          const p = framePositionsRef.current.get(id);
          if (p) return p;
          const f = currentFrames.find((x) => x.id === id);
          return f?.position ? { x: f.position.x, y: f.position.y } : null;
        };
        const getFrameSize = (id: number) => currentFrames.find((f) => f.id === id)?.size;
        const geometryOk =
          sourceFrame && tgtFrame
            ? wiringGeometryOk(
                wiring.sourceId,
                frameId,
                wiring.type,
                wiring.medium,
                currentFrames,
                currentConns,
                getPosition,
                getFrameSize,
                obstacleCellsRef.current,
              )
            : false;

        if (
          frameId !== wiring.sourceId &&
          tgtFrame &&
          hasConn &&
          hasReq &&
          cardOk &&
          pairOk &&
          geometryOk
        ) {
          createConnectionRef.current(rpcClientRef.current, wiring.sourceId, frameId, wiring.type, wiring.medium).catch(console.error);
        }
        wiringRef.current = null;
        setWiringMode(null);
        const wl = wiringLineRef.current;
        if (wl) wl.visible = false;
        return;
      }

      // Delegate to drag handler
      dragState.handleFramePointerDown(e, frameId);
    },
    [dragState],
  );

  // --- Handle frame context menu ---
  const handleFrameContextMenu = useCallback(
    (e: React.MouseEvent, frameId: number) => {
      e.preventDefault();
      e.stopPropagation();

      // If wiring mode: complete connection via right-click
      const wiring = wiringRef.current;
      if (wiring && frameId !== wiring.sourceId) {
        const currentFrames = framesRef.current;
        const currentConns = connectionsRef.current;
        const tgtFrame = currentFrames.find((f) => f.id === frameId);
        const hasConn = tgtFrame ? frameHasConnector(tgtFrame, wiring.type, wiring.medium) : false;
        const hasReq = tgtFrame
          ? frameMeetsConnectionRequirements(tgtFrame, wiring.type, wiring.medium)
          : false;
        const cardOk = isCardinallySatisfied(currentConns, currentFrames, frameId, wiring.type, wiring.medium);
        const sourceFrame = currentFrames.find((f) => f.id === wiring.sourceId);
        const getPosition = (id: number) => {
          const p = framePositionsRef.current.get(id);
          if (p) return p;
          const f = currentFrames.find((x) => x.id === id);
          return f?.position ? { x: f.position.x, y: f.position.y } : null;
        };
        const getFrameSize = (id: number) => currentFrames.find((f) => f.id === id)?.size;
        const geometryOk =
          sourceFrame && tgtFrame
            ? wiringGeometryOk(
                wiring.sourceId,
                frameId,
                wiring.type,
                wiring.medium,
                currentFrames,
                currentConns,
                getPosition,
                getFrameSize,
                obstacleCellsRef.current,
              )
            : false;

        if (tgtFrame && hasConn && hasReq && cardOk && geometryOk) {
          createConnectionRef.current(
            rpcClientRef.current,
            wiring.sourceId,
            frameId,
            wiring.type,
            wiring.medium,
          ).catch(console.error);
        }
        wiringRef.current = null;
        setWiringMode(null);
        const wl = wiringLineRef.current;
        if (wl) wl.visible = false;
        return;
      }

      // Check if a component badge was clicked
      const target = e.target as HTMLElement;
      const compEl = target.closest("[data-component-id]");
      if (compEl) {
        const componentId = Number(compEl.getAttribute("data-component-id"));
        if (!Number.isNaN(componentId)) {
          setCompContextMenu({ x: e.clientX, y: e.clientY, frameId, componentId });
          setFrameContextMenu(null);
          setBulkFrameContextMenu(null);
          setContextMenu(null);
          return;
        }
      }

      if (selectedFrameIdsRef.current.length > 1 && selectedFrameIdsRef.current.includes(frameId)) {
        setBulkFrameContextMenu({
          x: e.clientX,
          y: e.clientY,
          frameIds: [...selectedFrameIdsRef.current],
        });
        setFrameContextMenu(null);
        setCompContextMenu(null);
        setContextMenu(null);
        return;
      }

      setFrameContextMenu({ x: e.clientX, y: e.clientY, frameId });
      setBulkFrameContextMenu(null);
      setCompContextMenu(null);
      setContextMenu(null);
    },
    [],
  );

  const removeFramesBulk = useCallback(
    async (ids: number[]) => {
      if (ids.length === 0) return;
      const client = rpcClientRef.current;
      const connIds = new Set<number>();
      for (const c of useGameStore.getState().connections) {
        if (ids.includes(c.source) || ids.includes(c.target)) connIds.add(c.id);
      }
      for (const cid of connIds) {
        await removeConnection(client, cid);
      }
      for (const fid of ids) {
        if (wiringRef.current?.sourceId === fid) {
          wiringRef.current = null;
          setWiringMode(null);
          const wl = wiringLineRef.current;
          if (wl) wl.visible = false;
        }
        await removeFrame(client, fid);
      }
    },
    [removeConnection, removeFrame],
  );

  const onBackgroundClick = (event: FederatedPointerEvent) => {
    if (event.button !== 0) {
      return;
    }
    // If in wiring mode, cancel it
    if (wiringRef.current) {
      wiringRef.current = null;
      setWiringMode(null);
      const wl = wiringLineRef.current;
      if (wl) wl.visible = false;
      return;
    }
    // If in creation mode, place the frame
    if (creatingRef.current && worldRef.current) {
      const bp = creatingRef.current;
      const local = event.getLocalPosition(worldRef.current);
      const x = Math.round(local.x / CELL) * CELL;
      const y = Math.round(local.y / CELL) * CELL;
      const src = blueprintMapRef.current[bp] ?? "";
      const sizeKey = parseBlueprintSize(src);
      if (
        !isHypotheticalFramePlacementValid(
          { x, y },
          sizeKey,
          framesRef.current,
          connectionsRef.current,
          framePositionsRef.current,
          obstacleCellsRef.current,
        )
      ) {
        return;
      }
      exitCreationMode();
      void createFromBlueprint(rpcClient, bp, { x, y });
      return;
    }
    // Check for patch click
    const world = worldRef.current;
    if (world) {
      const pos = event.getLocalPosition(world);
      const clickedPatch = getPatchAtWorldPos(pos.x, pos.y);
      if (clickedPatch) {
        const { x: clientX, y: clientY } = getEventClientXY(event);
        setPatchInspector({ patch: clickedPatch, x: clientX, y: clientY });
        return;
      }
    }
    // Marquee selection start (clears selection on click-up if no drag — see pointerup handler)
    const app = appRef.current;
    const canvasEl = app?.canvas ?? null;
    const worldContainer = worldRef.current;
    const boxGfx = selectionBoxRef.current;
    if (!canvasEl || !worldContainer || !boxGfx) {
      selectFrame(null);
      return;
    }
    const { x: clientX, y: clientY } = getEventClientXY(event);
    const rect = canvasEl.getBoundingClientRect();
    const sx = clientX - rect.left;
    const sy = clientY - rect.top;
    const scale = worldContainer.scale.x;
    const wx = (sx - worldContainer.x) / scale;
    const wy = (sy - worldContainer.y) / scale;
    boxSelectSessionRef.current = {
      active: true,
      wx0: wx,
      wy0: wy,
    };
    boxGfx.clear();
    boxGfx.visible = true;

    const onMove = (pe: PointerEvent) => {
      const sess = boxSelectSessionRef.current;
      if (!sess?.active) return;
      const r = canvasEl.getBoundingClientRect();
      const csx = pe.clientX - r.left;
      const csy = pe.clientY - r.top;
      const curWx = (csx - worldContainer.x) / scale;
      const curWy = (csy - worldContainer.y) / scale;
      const x0 = Math.min(sess.wx0, curWx);
      const y0 = Math.min(sess.wy0, curWy);
      const x1 = Math.max(sess.wx0, curWx);
      const y1 = Math.max(sess.wy0, curWy);
      boxGfx.clear();
      boxGfx.rect(x0, y0, x1 - x0, y1 - y0);
      boxGfx.fill({ color: 0x3b82f6, alpha: 0.12 });
      boxGfx.stroke({ width: 1, color: 0x3b82f6, alpha: 0.95 });
    };

    const onUp = (pe: PointerEvent) => {
      const sess = boxSelectSessionRef.current;
      boxSelectSessionRef.current = null;
      boxGfx.clear();
      boxGfx.visible = false;
      window.removeEventListener("pointermove", onMove);
      window.removeEventListener("pointerup", onUp);

      if (!sess?.active) return;

      const r = canvasEl.getBoundingClientRect();
      const csx = pe.clientX - r.left;
      const csy = pe.clientY - r.top;
      const curWx = (csx - worldContainer.x) / scale;
      const curWy = (csy - worldContainer.y) / scale;
      const dragDist = Math.hypot(curWx - sess.wx0, curWy - sess.wy0);
      const minSide = 4 / scale;
      if (dragDist < minSide) {
        if (!pe.shiftKey) {
          selectFrame(null);
        }
        return;
      }

      const bx0 = Math.min(sess.wx0, curWx);
      const by0 = Math.min(sess.wy0, curWy);
      const bx1 = Math.max(sess.wx0, curWx);
      const by1 = Math.max(sess.wy0, curWy);

      const hit: number[] = [];
      for (const f of placedFramesRef.current) {
        const side = getFrameSquareSide(f);
        const fx = f._x;
        const fy = f._y;
        if (bx0 < fx + side && bx1 > fx && by0 < fy + side && by1 > fy) {
          hit.push(f.id);
        }
      }
      hit.sort((a, b) => a - b);
      if (hit.length === 0) {
        if (!pe.shiftKey) selectFrame(null);
        return;
      }
      if (pe.shiftKey) {
        const merged = [...new Set([...selectedFrameIdsRef.current, ...hit])];
        merged.sort((a, b) => a - b);
        setFrameSelection(merged);
      } else {
        setFrameSelection(hit);
      }
    };

    window.addEventListener("pointermove", onMove);
    window.addEventListener("pointerup", onUp);
  };

  /** Sync the HTML overlay transform with the Pixi worldContainer */
  const syncOverlayTransform = () => {
    const world = worldRef.current;
    if (!world || !overlayRef.current) return;
    overlayRef.current.syncTransform(world.scale.x, world.x, world.y);
  };

  useEffect(() => {
    const host = hostRef.current;
    if (!host) {
      return;
    }

    const app = new Application();
    let canvasEl: HTMLCanvasElement | null = null;
    let initialized = false;
    let disposed = false;
    let resizeObserver: ResizeObserver | null = null;

    void app
      .init({
        width: host.clientWidth || 1,
        height: host.clientHeight || 1,
        background: 0x0b1020,
        antialias: true,
      })
      .then(() => {
        if (disposed) {
          app.destroy();
          return;
        }
        initialized = true;
        canvasEl = app.canvas;
        host.appendChild(canvasEl);
        appRef.current = app;
        app.stage.eventMode = "static";
        app.stage.hitArea = app.screen;

        const worldContainer = new Container();
        worldRef.current = worldContainer;
        app.stage.addChild(worldContainer);

        const grid = new Graphics();
        gridRef.current = grid;
        worldContainer.addChild(grid);

        const restrictedDropLayer = new Graphics();
        restrictedDropLayerRef.current = restrictedDropLayer;
        worldContainer.addChild(restrictedDropLayer);

        const patchLayer = new Container();
        patchLayer.label = "patches";
        worldContainer.addChild(patchLayer);

        const mLayer = new Graphics();
        markerLayerRef.current = mLayer;
        worldContainer.addChild(mLayer);
        patchLayerRef.current = patchLayer;

        const connectionLayer = new Graphics();
        connectionLayerRef.current = connectionLayer;
        worldContainer.addChild(connectionLayer);

        const selectionBox = new Graphics();
        selectionBox.visible = false;
        selectionBoxRef.current = selectionBox;
        worldContainer.addChild(selectionBox);

        // No more Pixi frameLayer — frames are rendered as HTML overlays

        redrawGrid(grid, worldContainer, app.screen.width, app.screen.height);

        // Initial overlay sync
        syncOverlayTransform();

        const onConnAnimTick = () => {
          const conns = useGameStore.getState().connections;
          const anyWireless = conns.some((c) => (c.medium ?? "WIRE") === "WIRELESS");
          const anyConveyorAnim = conns.some(
            (c) =>
              c.type === "CONVEYOR" &&
              typeof c.transfer_progress === "number" &&
              c.transfer_progress > 0.02 &&
              c.transfer_progress < 0.98,
          );
          if (anyWireless || anyConveyorAnim) drawConnectionsRef.current();
        };
        app.ticker.add(onConnAnimTick);

        // --- ResizeObserver ---
        resizeObserver = new ResizeObserver((entries) => {
          for (const entry of entries) {
            const { width, height } = entry.contentRect;
            if (width > 0 && height > 0) {
              app.renderer.resize(width, height);
              app.stage.hitArea = app.screen;
              redrawGrid(grid, worldContainer, width, height);
            }
          }
        });
        resizeObserver.observe(host);

        // --- Wheel zoom ---
        const onWheel = (e: WheelEvent) => {
          e.preventDefault();
          const rect = canvasEl!.getBoundingClientRect();
          const sx = e.clientX - rect.left;
          const sy = e.clientY - rect.top;

          const oldScale = worldContainer.scale.x;
          const direction = e.deltaY < 0 ? 1 : -1;
          const newScale = Math.min(MAX_ZOOM, Math.max(MIN_ZOOM, oldScale * (1 + direction * ZOOM_FACTOR)));

          const worldX = (sx - worldContainer.x) / oldScale;
          const worldY = (sy - worldContainer.y) / oldScale;

          worldContainer.scale.set(newScale);
          worldContainer.x = sx - worldX * newScale;
          worldContainer.y = sy - worldY * newScale;

          redrawGrid(grid, worldContainer, app.screen.width, app.screen.height);
          syncOverlayTransform();
          setZoom(newScale);
        };
        canvasEl.addEventListener("wheel", onWheel, { passive: false });

        // --- Middle-click pan ---
        const onPanStart = (e: PointerEvent) => {
          if (e.button !== 1) return;
          e.preventDefault();
          panningRef.current = true;
          panStartRef.current = {
            sx: e.clientX,
            sy: e.clientY,
            wx: worldContainer.x,
            wy: worldContainer.y,
          };
          canvasEl!.setPointerCapture(e.pointerId);
        };
        const onPanMove = (e: PointerEvent) => {
          if (!panningRef.current) return;
          const dx = e.clientX - panStartRef.current.sx;
          const dy = e.clientY - panStartRef.current.sy;
          worldContainer.x = panStartRef.current.wx + dx;
          worldContainer.y = panStartRef.current.wy + dy;
          redrawGrid(grid, worldContainer, app.screen.width, app.screen.height);
          syncOverlayTransform();
        };
        const onPanEnd = () => {
          panningRef.current = false;
        };

        canvasEl.addEventListener("pointerdown", onPanStart);
        canvasEl.addEventListener("pointermove", onPanMove);
        canvasEl.addEventListener("pointerup", onPanEnd);
        canvasEl.addEventListener("pointercancel", onPanEnd);

        // Suppress middle-click default (auto-scroll)
        const onAuxClick = (e: MouseEvent) => {
          if (e.button === 1) e.preventDefault();
        };
        canvasEl.addEventListener("auxclick", onAuxClick);

        // --- Right-click context menu (background only — frame right-clicks handled by FrameCard) ---
        const onContextMenu = (e: MouseEvent) => {
          e.preventDefault();
          const rect = canvasEl!.getBoundingClientRect();
          const sx = e.clientX - rect.left;
          const sy = e.clientY - rect.top;
          const scale = worldContainer.scale.x;
          const rawWx = (sx - worldContainer.x) / scale;
          const rawWy = (sy - worldContainer.y) / scale;
          const wx = Math.round(rawWx / CELL) * CELL;
          const wy = Math.round(rawWy / CELL) * CELL;

          // Background context menu (no frame hit-testing needed — HTML handles frame clicks)
          setContextMenu({ x: sx, y: sy, wx, wy });
          setFrameContextMenu(null);
          setBulkFrameContextMenu(null);
          setCompContextMenu(null);
        };
        canvasEl.addEventListener("contextmenu", onContextMenu);

        // --- Dispatch Mouse Coordinates ---
        let lastMouseSend = 0;
        const onPointerMove = (e: PointerEvent) => {
          const now = Date.now();
          if (now - lastMouseSend > 100) { // Throttle to 10Hz
            lastMouseSend = now;
            const rect = canvasEl!.getBoundingClientRect();
            const sx = e.clientX - rect.left;
            const sy = e.clientY - rect.top;
            const scale = worldContainer.scale.x;
            const rawWx = (sx - worldContainer.x) / scale;
            const rawWy = (sy - worldContainer.y) / scale;
            void rpcClientRef.current.call("input.mouse_coords", { x: rawWx, y: rawWy }).catch(() => {});
          }
        };
        canvasEl.addEventListener("pointermove", onPointerMove);

        // --- Ghost preview for creation mode ---
        const ghost = new Graphics();
        ghost.visible = false;
        ghost.alpha = 0.4;
        ghostRef.current = ghost;
        worldContainer.addChild(ghost);

        // --- Wiring line preview ---
        const wiringLine = new Graphics();
        wiringLine.visible = false;
        wiringLineRef.current = wiringLine;
        worldContainer.addChild(wiringLine);

        const onCreationMove = (e: PointerEvent) => {
          const rect = canvasEl!.getBoundingClientRect();
          const sx = e.clientX - rect.left;
          const sy = e.clientY - rect.top;
          const scale = worldContainer.scale.x;
          const cursorWx = (sx - worldContainer.x) / scale;
          const cursorWy = (sy - worldContainer.y) / scale;

          if (creatingRef.current) {
            const wx = Math.round(cursorWx / CELL) * CELL;
            const wy = Math.round(cursorWy / CELL) * CELL;
            const src = blueprintMapRef.current[creatingRef.current] ?? "";
            const sizeKey = parseBlueprintSize(src);
            const cells = FRAME_CELL_SIZES[sizeKey] ?? 2;
            const boxSize = cells * CELL;
            const valid = isHypotheticalFramePlacementValid(
              { x: wx, y: wy },
              sizeKey,
              framesRef.current,
              connectionsRef.current,
              framePositionsRef.current,
              obstacleCellsRef.current,
            );
            ghost.clear();
            ghost.beginFill(valid ? 0x3b82f6 : 0xff3355);
            ghost.roundRect(0, 0, boxSize, boxSize, 10);
            ghost.fill();
            ghost.x = wx;
            ghost.y = wy;
            ghost.visible = true;
          } else if (ghost.visible) {
            ghost.visible = false;
          }

          // Update wiring preview: max-distance circle + line to cursor
          const wiring = wiringRef.current;
          if (wiring) {
            const sourceFrame = placedFramesRef.current.find((f) => f.id === wiring.sourceId);
            if (sourceFrame) {
              const srcCells = FRAME_CELL_SIZES[sourceFrame.size] ?? 1;
              const boxSize = srcCells * CELL;
              const cx = sourceFrame._x + boxSize / 2;
              const cy = sourceFrame._y + boxSize / 2;
              const socket = edgeExitTowardPoint(
                { x: sourceFrame._x, y: sourceFrame._y },
                sourceFrame.size,
                { x: cursorWx, y: cursorWy },
              );
              const color = CONN_COLOR[wiring.type] ?? 0xffffff;
              const medium = wiring.medium ?? "WIRE";
              const rawR = getMaxConnectionDistanceForFrame(sourceFrame, wiring.type, medium);
              const radius =
                Number.isFinite(rawR) && rawR > 0 && rawR < 1e15 ? rawR : 1000;
              wiringLine.clear();
              wiringLine.setStrokeStyle({ width: 1, color, alpha: 0.42 });
              wiringLine.circle(cx, cy, radius);
              wiringLine.stroke();
              wiringLine.setStrokeStyle({ width: 2, color, alpha: 0.85 });
              wiringLine.moveTo(socket.x, socket.y);
              wiringLine.lineTo(cursorWx, cursorWy);
              wiringLine.stroke();
              wiringLine.circle(socket.x, socket.y, 5);
              wiringLine.fill({ color, alpha: 0.35 });
              wiringLine.stroke({ width: 2, color, alpha: 0.95 });
              wiringLine.visible = true;
            }
          } else if (wiringLine.visible) {
            wiringLine.visible = false;
          }
        };
        canvasEl.addEventListener("pointermove", onCreationMove);

        const onCreationKeydown = (e: KeyboardEvent) => {
          if (e.key === "Escape") {
            if (creatingRef.current) {
              setCreationBlueprint(null);
              creatingRef.current = null;
              ghost.visible = false;
            }
            if (wiringRef.current) {
              wiringRef.current = null;
              setWiringMode(null);
              wiringLine.visible = false;
            }
          }
        };
        window.addEventListener("keydown", onCreationKeydown);

        // Store cleanup references on the canvas element
        (canvasEl as any).__canvasCleanup = () => {
          app.ticker.remove(onConnAnimTick);
          canvasEl!.removeEventListener("wheel", onWheel);
          canvasEl!.removeEventListener("pointerdown", onPanStart);
          canvasEl!.removeEventListener("pointermove", onPanMove);
          canvasEl!.removeEventListener("pointerup", onPanEnd);
          canvasEl!.removeEventListener("pointercancel", onPanEnd);
          canvasEl!.removeEventListener("auxclick", onAuxClick);
          canvasEl!.removeEventListener("contextmenu", onContextMenu);
          canvasEl!.removeEventListener("pointermove", onCreationMove);
          canvasEl!.removeEventListener("pointermove", onPointerMove);
          window.removeEventListener("keydown", onCreationKeydown);
        };
      });

    return () => {
      disposed = true;
      appRef.current = null;
      worldRef.current = null;
      gridRef.current = null;
      restrictedDropLayerRef.current = null;
      patchLayerRef.current = null;
      markerLayerRef.current = null;
      connectionLayerRef.current = null;
      selectionBoxRef.current = null;
      wiringLineRef.current = null;
      panningRef.current = false;
      if (resizeObserver) resizeObserver.disconnect();
      if (canvasEl) {
        (canvasEl as any).__canvasCleanup?.();
        if (host.contains(canvasEl)) host.removeChild(canvasEl);
      }
      if (initialized) {
        app.destroy();
      }
    };
  }, []);

  useEffect(() => {
    drawConnections();
  }, [connections, powerNetworks]); // eslint-disable-line react-hooks/exhaustive-deps

  // --- Re-sync frame positions and redraw connections when frames change ---
  useEffect(() => {
    // Stage-level background click handler
    const app = appRef.current;
    if (!app) return;

    app.stage.off("pointerdown");
    app.stage.on("pointerdown", (e: FederatedPointerEvent) => {
      if (e.button !== 0) return;

      // If we're wiring, handle cancel at stage level
      const wiring = wiringRef.current;
      if (wiring) {
        // Clicked while wiring but didn't hit a frame card (HTML handles frame clicks)
        wiringRef.current = null;
        setWiringMode(null);
        const wl = wiringLineRef.current;
        if (wl) wl.visible = false;
        return;
      }

      void onBackgroundClick(e);
    });

    drawConnectionsRef.current();
    // Re-sync overlay card positions after React renders new cards
    syncOverlayTransform();
  }, [onBackgroundClick, placedFrames, selectFrame, selectedFrameId, wiringMode]);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== "Delete" && e.key !== "Backspace") return;
      const el = e.target as HTMLElement | null;
      if (el?.closest("input, textarea, select, [contenteditable=true]")) return;
      const ids = [...useGameStore.getState().selectedFrameIds];
      if (ids.length === 0) return;
      e.preventDefault();
      const n = ids.length;
      if (!window.confirm(`Remove ${n} frame${n === 1 ? "" : "s"}? Connections will be removed.`)) return;
      void removeFramesBulk(ids).catch(console.error);
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [removeFramesBulk]);

  return (
    <div ref={hostRef} style={{ width: "100%", height: "100%", position: "relative" }}>
      {/* HTML frame overlay — synced with Pixi worldContainer transform */}
      <FrameOverlay
        ref={overlayRef}
        placedFrames={placedFrames}
        selectedFrameIds={selectedFrameIds}
        wiringMode={wiringMode}
        connections={connections}
        frames={placedFrames}
        obstacleCells={obstacleCells}
        zoom={zoom}
        onFramePointerDown={handleFramePointerDown}
        onFrameContextMenu={handleFrameContextMenu}
      />

      {creationBlueprint && (
        <div
          style={{
            position: "absolute",
            top: 6,
            left: "50%",
            transform: "translateX(-50%)",
            background: "#1e293b",
            border: "1px solid #3b82f6",
            borderRadius: 6,
            padding: "4px 12px",
            fontSize: 12,
            color: "#e5e7eb",
            zIndex: 20,
            pointerEvents: "none",
            userSelect: "none",
          }}
        >
          Placing <b>{creationBlueprint}</b> — click to place, Esc to cancel
        </div>
      )}
      {wiringMode && (
        <div
          style={{
            position: "absolute",
            top: 6,
            left: "50%",
            transform: "translateX(-50%)",
            background: "#1e293b",
            border: `1px solid ${toCssHex(CONN_COLOR[wiringMode.type] ?? 0x3b82f6)}`,
            borderRadius: 6,
            padding: "4px 12px",
            fontSize: 12,
            color: "#e5e7eb",
            zIndex: 20,
            pointerEvents: "none",
            userSelect: "none",
          }}
        >
          Connect{" "}
          <b style={{ color: toCssHex(CONN_COLOR[wiringMode.type] ?? 0x3b82f6) }}>{wiringMode.type}</b>{" "}
          from{" "}
          <b>{frames.find((f) => f.id === wiringMode.sourceId)?.name ?? `Frame ${wiringMode.sourceId}`}</b>{" "}
          — click target frame, Esc to cancel
        </div>
      )}
      {wiringMode && (() => {
        const sourceId = wiringMode.sourceId;
        const conns = connections;
        const medium = wiringMode.medium ?? "WIRE";
        const sourceFrame = frames.find((f) => f.id === sourceId);
        const getPosition = (id: number) => {
          const f = placedFrames.find((x) => x.id === id);
          return f ? { x: f._x, y: f._y } : null;
        };
        const getFrameSize = (id: number) => placedFrames.find((f) => f.id === id)?.size;
        const eligibleTargets = frames.filter((frame) => {
          if (frame.id === sourceId) return false;
          if (!sourceFrame) return false;
          if (
            !wiringGeometryOk(
              sourceId,
              frame.id,
              wiringMode.type,
              medium,
              frames,
              conns,
              getPosition,
              getFrameSize,
              obstacleCells,
            )
          )
            return false;
          if (!frameHasConnector(frame, wiringMode.type, wiringMode.medium ?? "WIRE")) return false;
          if (!frameMeetsConnectionRequirements(frame, wiringMode.type, wiringMode.medium ?? "WIRE")) return false;
          if (!isCardinallySatisfied(conns, frames, frame.id, wiringMode.type, wiringMode.medium ?? "WIRE")) return false;
          if (hasConnectionBetween(conns, sourceId, frame.id, wiringMode.type, wiringMode.medium ?? "WIRE")) return false;
          return true;
        });
        if (eligibleTargets.length > 0) return null;

        let reason = "No eligible target frames for this connection.";
        if (wiringMode.type === "CONVEYOR") {
          reason = "Conveyor connections require a frame with item storage (e.g. Storage or Big Storage).";
        } else if (wiringMode.type === "DATA") {
          reason = "Data connections require a frame with a Core component.";
        }

        return (
          <div
            style={{
              position: "absolute",
              bottom: 10,
              left: "50%",
              transform: "translateX(-50%)",
              background: "#111827",
              border: "1px solid #374151",
              borderRadius: 6,
              padding: "4px 10px",
              fontSize: 11,
              color: "#e5e7eb",
              zIndex: 20,
              pointerEvents: "none",
              userSelect: "none",
              maxWidth: 420,
              textAlign: "center",
            }}
          >
            {reason}
          </div>
        );
      })()}
      {contextMenu && (
        <div
          data-context-menu
          style={{
            position: "absolute",
            top: contextMenu.y,
            left: contextMenu.x,
            background: "#1e293b",
            border: "1px solid #334155",
            borderRadius: 6,
            padding: "4px 0",
            fontSize: 12,
            color: "#e5e7eb",
            zIndex: 30,
            minWidth: 160,
            boxShadow: "0 4px 16px rgba(0,0,0,0.5)",
          }}
        >
          <div
            style={{ padding: "4px 10px", cursor: "pointer" }}
            onMouseEnter={(e) => (e.currentTarget.style.background = "#334155")}
            onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
            onClick={() => {
              setBlueprintPickerOpen({ wx: contextMenu.wx, wy: contextMenu.wy });
              setContextMenu(null);
            }}
          >
            Create Frame...
          </div>
          <div
            style={{ padding: "4px 10px", cursor: "pointer" }}
            onMouseEnter={(e) => (e.currentTarget.style.background = "#334155")}
            onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
            onClick={() => {
              const { wx, wy } = contextMenu;
              setContextMenu(null);
              const def = `Marker ${Math.round(wx)}, ${Math.round(wy)}`;
              const label = window.prompt("Marker name", def);
              const trimmed = label?.trim();
              if (trimmed) {
                void rpcClient.call("input.marker_set", {
                  x: wx,
                  y: wy,
                  label: trimmed,
                  color: "#ff0000",
                });
              }
            }}
          >
            Add marker...
          </div>
          {patchTypes.length > 0 && (
            <>
              <div style={{ borderTop: "1px solid #374151", margin: "4px 0" }} />
              <div style={{ padding: "4px 8px", color: "#9ca3af", fontSize: "11px" }}>Resource Patches</div>
              {patchTypes.map((pt) => (
                <button
                  key={pt.key}
                  style={{
                    display: "block",
                    width: "100%",
                    padding: "6px 12px",
                    background: "none",
                    border: "none",
                    color: "#d1d5db",
                    textAlign: "left",
                    cursor: "pointer",
                  }}
                  onMouseEnter={(e) => (e.currentTarget.style.background = "#334155")}
                  onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
                  onClick={() => createPatch(pt.key, contextMenu.wx, contextMenu.wy)}
                >
                  Create {pt.name}
                </button>
              ))}
            </>
          )}
        </div>
      )}
      {bulkFrameContextMenu &&
        createPortal(
          <div
            data-context-menu
            style={{
              position: "fixed",
              top: bulkFrameContextMenu.y,
              left: bulkFrameContextMenu.x,
              background: "#1e293b",
              border: "1px solid #334155",
              borderRadius: 6,
              padding: "4px 0",
              fontSize: 12,
              color: "#e5e7eb",
              zIndex: 9999,
              minWidth: 190,
              boxShadow: "0 4px 16px rgba(0,0,0,0.5)",
            }}
          >
            <div
              style={{
                padding: "4px 10px 6px",
                fontSize: 11,
                color: "#94a3b8",
                fontWeight: 600,
                borderBottom: "1px solid #1e3a5f",
              }}
            >
              {bulkFrameContextMenu.frameIds.length} frames selected
            </div>
            <div
              style={{ padding: "4px 10px", cursor: "pointer", color: "#f87171" }}
              onMouseEnter={(e) => (e.currentTarget.style.background = "#334155")}
              onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
              onClick={() => {
                const ids = bulkFrameContextMenu.frameIds;
                if (!window.confirm(`Remove ${ids.length} frames? Connections will be removed.`)) return;
                setBulkFrameContextMenu(null);
                void removeFramesBulk(ids).catch(console.error);
              }}
            >
              Remove frames…
            </div>
          </div>,
          document.body,
        )}
      {frameContextMenu && (() => {
        const frame = frames.find((f) => f.id === frameContextMenu.frameId);
        const allConnInfo = (["POWER", "DATA", "CONVEYOR"] as const).map((type) => {
          if (!frame) {
            return { type, available: false as const, reason: "Frame not found." };
          }
          if (!frameHasConnector(frame, type, "WIRE")) {
            return { type, available: false as const, reason: "No connector component on this frame." };
          }
          if (!frameMeetsConnectionRequirements(frame, type, "WIRE")) {
            let reason = "Frame does not meet connection requirements.";
            if (type === "CONVEYOR") {
              reason = "Requires a component with item storage on this frame.";
            } else if (type === "DATA") {
              reason = "Requires a Core component on this frame.";
            }
            return { type, available: false as const, reason };
          }
          if (!isCardinallySatisfied(connections, frames, frame.id, type, "WIRE")) {
            return { type, available: false as const, reason: "Max connections reached for this frame." };
          }
          return { type, available: true as const, reason: "" };
        });
        const connTypes = allConnInfo.filter((c) => c.available).map((c) => c.type);
        const existingConns = connections.filter(
          (c) => c.source === frameContextMenu.frameId || c.target === frameContextMenu.frameId,
        );
        return createPortal(
          <div
            data-context-menu
            style={{
              position: "fixed",
              top: frameContextMenu.y,
              left: frameContextMenu.x,
              background: "#1e293b",
              border: "1px solid #334155",
              borderRadius: 6,
              padding: "4px 0",
              fontSize: 12,
              color: "#e5e7eb",
              zIndex: 9999,
              minWidth: 190,
              boxShadow: "0 4px 16px rgba(0,0,0,0.5)",
            }}
          >
            <div
              style={{
                padding: "4px 10px 6px",
                fontSize: 11,
                color: "#94a3b8",
                fontWeight: 600,
                borderBottom: "1px solid #1e3a5f",
              }}
            >
              {frameRenaming?.frameId === frameContextMenu.frameId ? (
                <form
                  style={{ display: "flex", gap: 4, alignItems: "center" }}
                  onSubmit={(e) => {
                    e.preventDefault();
                    void updateFrameMetadata(rpcClient, frameContextMenu.frameId, { name: frameRenaming.name });
                    setFrameRenaming(null);
                  }}
                >
                  <input
                    autoFocus
                    value={frameRenaming.name}
                    onChange={(e) => setFrameRenaming({ ...frameRenaming, name: e.target.value })}
                    onKeyDown={(e) => {
                      if (e.key === "Escape") {
                        setFrameRenaming(null);
                      }
                    }}
                    onClick={(e) => e.stopPropagation()}
                    style={{
                      flex: 1,
                      fontSize: 11,
                      padding: "2px 4px",
                      background: "#0f172a",
                      border: "1px solid #334155",
                      borderRadius: 3,
                      color: "#e5e7eb",
                      minWidth: 80,
                    }}
                  />
                  <button
                    type="submit"
                    style={{
                      fontSize: 10,
                      padding: "2px 6px",
                      background: "#1e3a5f",
                      border: "1px solid #334155",
                      borderRadius: 3,
                      color: "#e5e7eb",
                      cursor: "pointer",
                    }}
                  >
                    Save
                  </button>
                  <button
                    type="button"
                    onClick={() => setFrameRenaming(null)}
                    style={{
                      fontSize: 10,
                      padding: "2px 6px",
                      background: "transparent",
                      border: "1px solid #334155",
                      borderRadius: 3,
                      color: "#9ca3af",
                      cursor: "pointer",
                    }}
                  >
                    ✕
                  </button>
                </form>
              ) : (
                <>
                  {frame?.name ?? `Frame ${frameContextMenu.frameId}`}
                  <span style={{ color: "#475569", marginLeft: 6 }}>#{frameContextMenu.frameId}</span>
                </>
              )}
            </div>
            {frameRenaming?.frameId !== frameContextMenu.frameId && (
              <div
                style={{ padding: "4px 10px", cursor: "pointer" }}
                onMouseEnter={(e) => (e.currentTarget.style.background = "#334155")}
                onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
                onClick={() => {
                  setFrameRenaming({ frameId: frameContextMenu.frameId, name: frame?.name ?? "" });
                }}
              >
                Rename
              </div>
            )}
            <div
              style={{ padding: "4px 10px", cursor: "pointer" }}
              onMouseEnter={(e) => (e.currentTarget.style.background = "#334155")}
              onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
              onClick={() => {
                setComponentPickerOpen({ frameId: frameContextMenu.frameId });
                setFrameContextMenu(null);
              }}
            >
              Add Component...
            </div>
            {/* Accent color picker */}
            <div style={{ padding: "4px 10px", fontSize: 11, color: "#9ca3af", fontWeight: 600 }}>Color</div>
            <div style={{ padding: "2px 10px", display: "flex", gap: 4, flexWrap: "wrap" }}>
              {[null, "#ef4444", "#f97316", "#eab308", "#22c55e", "#06b6d4", "#3b82f6", "#8b5cf6", "#ec4899"].map((color) => (
                <button
                  key={color ?? "none"}
                  type="button"
                  title={color ?? "No color"}
                  style={{
                    width: 16,
                    height: 16,
                    borderRadius: 3,
                    border: `1px solid ${
                      (frame?.metadata?.attributes?.accent?.base_value ?? "") === (color ?? "")
                        ? "#e5e7eb"
                        : "#475569"
                    }`,
                    background: color ?? "#1e293b",
                    cursor: "pointer",
                    padding: 0,
                    position: "relative",
                  }}
                  onClick={() => {
                    void updateFrameMetadata(rpcClient, frameContextMenu.frameId, {
                      attributes: { accent: color ?? "" },
                    });
                    setFrameContextMenu(null);
                  }}
                >
                  {!color && (
                    <span style={{
                      position: "absolute",
                      inset: 0,
                      display: "flex",
                      alignItems: "center",
                      justifyContent: "center",
                      fontSize: 9,
                      color: "#64748b",
                    }}>✕</span>
                  )}
                </button>
              ))}
            </div>
            <div style={{ padding: "4px 10px", fontSize: 11, color: "#9ca3af", fontWeight: 600 }}>Connections</div>
            {connTypes.length === 0 ? (
              <div style={{ padding: "4px 10px", color: "#6b7280" }}>
                No connectors available
                {allConnInfo
                  .filter((c) => !c.available && c.reason && frame && frameHasConnector(frame, c.type, "WIRE"))
                  .map((c) => (
                    <div key={c.type} style={{ marginTop: 2, fontSize: 10, color: "#9ca3af" }}>
                      {c.type}: {c.reason}
                    </div>
                  ))}
              </div>
            ) : (
              connTypes.map((type) => (
                <div
                  key={type}
                  style={{ padding: "4px 10px", cursor: "pointer", display: "flex", alignItems: "center", gap: 8 }}
                  onMouseEnter={(e) => (e.currentTarget.style.background = "#334155")}
                  onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
                  onClick={() => {
                    wiringRef.current = { sourceId: frameContextMenu.frameId, type, medium: "WIRE" };
                    setWiringMode({ sourceId: frameContextMenu.frameId, type, medium: "WIRE" });
                    setFrameContextMenu(null);
                  }}
                >
                  <span
                    style={{
                      width: 8,
                      height: 8,
                      borderRadius: "50%",
                      background: toCssHex(CONN_COLOR[type] ?? 0xffffff),
                      flexShrink: 0,
                      display: "inline-block",
                    }}
                  />
                  <span>Add {type} connection</span>
                </div>
              ))
            )}
            {existingConns.length > 0 && (
              <>
                <div
                  style={{
                    padding: "6px 10px 4px",
                    fontSize: 11,
                    color: "#9ca3af",
                    fontWeight: 600,
                    borderTop: "1px solid #334155",
                    marginTop: 2,
                  }}
                >
                  Existing
                </div>
                {existingConns.map((conn) => (
                  <div
                    key={conn.id}
                    style={{
                      padding: "2px 10px",
                      display: "flex",
                      alignItems: "center",
                      gap: 6,
                      fontSize: 11,
                      justifyContent: "space-between",
                    }}
                  >
                    <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
                      <span
                        style={{
                          width: 7,
                          height: 7,
                          borderRadius: "50%",
                          background: toCssHex(CONN_COLOR[conn.type] ?? 0xffffff),
                          flexShrink: 0,
                          display: "inline-block",
                        }}
                      />
                      <span style={{ color: "#94a3b8" }}>
                        {conn.type}: {conn.source} → {conn.target}
                      </span>
                    </div>
                    <button
                      type="button"
                      style={{
                        border: "none",
                        background: "transparent",
                        color: "#f97373",
                        fontSize: 11,
                        cursor: "pointer",
                        padding: "2px 4px",
                      }}
                      onClick={() => {
                        void removeConnection(rpcClientRef.current, conn.id);
                      }}
                    >
                      Remove
                    </button>
                  </div>
                ))}
              </>
            )}
            <div style={{ borderTop: "1px solid #334155", marginTop: 4 }} />
            <div
              style={{ padding: "4px 10px", cursor: "pointer", color: "#f87171" }}
              onMouseEnter={(e) => (e.currentTarget.style.background = "#334155")}
              onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
              onClick={() => {
                const fid = frameContextMenu.frameId;
                if (
                  !window.confirm(
                    "Remove this frame? All connections to it will be destroyed.",
                  )
                ) {
                  return;
                }
                setFrameContextMenu(null);
                void (async () => {
                  const client = rpcClientRef.current;
                  if (wiringRef.current?.sourceId === fid) {
                    wiringRef.current = null;
                    setWiringMode(null);
                    const wl = wiringLineRef.current;
                    if (wl) wl.visible = false;
                  }
                  const toRemove = connections.filter(
                    (c) => c.source === fid || c.target === fid,
                  );
                  for (const c of toRemove) {
                    await removeConnection(client, c.id);
                  }
                  await removeFrame(client, fid);
                })().catch(console.error);
              }}
            >
              Remove frame…
            </div>
          </div>,
          document.body
        );
      })()}
      {compContextMenu && (() => {
        const ctxFrame = frames.find((f) => f.id === compContextMenu.frameId);
        const ctxComp = ctxFrame?.components?.find((c) => c.id === compContextMenu.componentId);
        if (!ctxFrame || !ctxComp) return null;
        return (
          <ComponentContextMenu
            x={compContextMenu.x}
            y={compContextMenu.y}
            frameId={compContextMenu.frameId}
            componentId={compContextMenu.componentId}
            component={ctxComp}
            onClose={() => setCompContextMenu(null)}
            rpcClient={rpcClient}
          />
        );
      })()}
      {componentPickerOpen && (
        <ComponentPicker
          isOpen={true}
          onClose={() => setComponentPickerOpen(null)}
          frameId={componentPickerOpen.frameId}
          rpcClient={rpcClient}
        />
      )}
      {blueprintPickerOpen && (
        <BlueprintPicker
          isOpen={true}
          onClose={() => setBlueprintPickerOpen(null)}
          rpcClient={rpcClient}
          onSelect={(blueprint) => {
            enterCreationMode(blueprint);
            setBlueprintPickerOpen(null);
          }}
        />
      )}
      {patchInspector && (
        <PatchMiniInspector
          patch={patchInspector.patch}
          x={patchInspector.x}
          y={patchInspector.y}
          rpcClient={rpcClient}
          onClose={() => setPatchInspector(null)}
        />
      )}
    </div>
  );
}
