import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { Application, Container, Graphics } from "pixi.js";
import type { FederatedPointerEvent } from "pixi.js";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { usePatchStore, Patch } from "../../stores/patches";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import type { ComponentDTO } from "../../rpc/types";
import { ComponentContextMenu } from "./ComponentContextMenu";
import { PatchMiniInspector } from "./PatchMiniInspector";
import { ComponentPicker, BlueprintPicker } from "../picker";
import { FrameOverlay, type FrameOverlayHandle } from "./FrameOverlay";
import { useFrameDrag } from "./useFrameDrag";
import type { PlacedFrame } from "./FrameCard";

type Props = {
  rpcClient: RpcClient;
  onFrameMiniInspect?: (frameId: number, clientX: number, clientY: number) => void;
};

const SUB_CELL = 25;
const CELL = SUB_CELL * 3;
const MIN_ZOOM = 0.15;
const MAX_ZOOM = 3.0;
const ZOOM_FACTOR = 0.1;
const MINOR_LINE_COLOR = 0x1a2332;
const MINOR_LINE_ALPHA = 0.25;
const MAJOR_LINE_COLOR = 0x1f2937;
const MAJOR_LINE_ALPHA = 0.4;
const GRID_DOT_COLOR = 0x374151;
const GRID_DOT_ALPHA = 0.5;
const GRID_DOT_RADIUS = 1.5;
const MINOR_HIDE_THRESHOLD = 6;
const MAJOR_HIDE_THRESHOLD = 4;

const FRAME_CELL_SIZES: Record<string, number> = {
  S: 1,
  M: 2,
  L: 3,
  G: 4,
};

const CONN_COLOR: Record<string, number> = {
  POWER: 0xeab308,
  DATA: 0x3b82f6,
  CONVEYOR: 0x22c55e,
};

const CONNECTOR_COMPONENT: Record<string, string> = {
  POWER: "Power Wire Connector",
  DATA: "Data Wire Connector",
  CONVEYOR: "Conveyor Connector",
};

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
): boolean {
  const compName = CONNECTOR_COMPONENT[type];
  if (!compName) return false;
  return frameHasComponentByName(frame, compName);
}

function frameMeetsConnectionRequirements(
  frame: { components?: { name: string }[] },
  type: string,
): boolean {
  if (type === "CONVEYOR") {
    return frameHasComponentByName(frame, "Storage");
  }
  if (type === "DATA") {
    return frameHasComponentByName(frame, "Core");
  }
  return true;
}

function getMaxConnectionsForFrameType(
  frames: { id: number; components?: ComponentDTO[] }[],
  frameId: number,
  type: string,
): number {
  const frame = frames.find((f) => f.id === frameId);
  if (!frame) return type === "POWER" ? 10 : 1;
  const connectorName = CONNECTOR_COMPONENT[type];
  const connector = (frame.components ?? []).find((c) => c.name === connectorName);
  const attr =
    connector?.metadata?.attributes?.max_connections ??
    (connector?.attributes as Record<string, unknown> | undefined)?.max_connections;
  if (attr && typeof (attr as any).final_value !== "undefined") {
    const v = (attr as any).final_value;
    if (typeof v === "number") return v;
    const parsed = Number(v);
    if (!Number.isNaN(parsed)) return parsed;
  }
  return type === "POWER" ? 10 : 1;
}

function getConnectionCountForFrameType(
  conns: { source: number; target: number; type: string }[],
  frameId: number,
  type: string,
): number {
  return conns.filter(
    (c) => (c.source === frameId || c.target === frameId) && c.type === type,
  ).length;
}

function isCardinallySatisfied(
  conns: { source: number; target: number; type: string }[],
  frames: { id: number; components?: ComponentDTO[] }[],
  frameId: number,
  type: string,
): boolean {
  const max = getMaxConnectionsForFrameType(frames, frameId, type);
  if (max <= 0) return false;
  const count = getConnectionCountForFrameType(conns, frameId, type);
  return count < max;
}

function hasConnectionBetween(
  conns: { source: number; target: number; type: string }[],
  a: number,
  b: number,
  type: string,
): boolean {
  return conns.some(
    (c) =>
      ((c.source === a && c.target === b) || (c.source === b && c.target === a)) &&
      c.type === type,
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

  // --- Minor grid lines (SUB_CELL spacing) ---
  const minorPx = SUB_CELL * scale;
  if (minorPx >= MINOR_HIDE_THRESHOLD) {
    const sxMin = Math.floor(wx0 / SUB_CELL) * SUB_CELL;
    const sxMax = Math.ceil(wx1 / SUB_CELL) * SUB_CELL;
    const syMin = Math.floor(wy0 / SUB_CELL) * SUB_CELL;
    const syMax = Math.ceil(wy1 / SUB_CELL) * SUB_CELL;

    grid.setStrokeStyle({ width: 1, color: MINOR_LINE_COLOR, alpha: MINOR_LINE_ALPHA });
    for (let x = sxMin; x <= sxMax; x += SUB_CELL) {
      if (x % CELL === 0) continue;
      grid.moveTo(x, syMin);
      grid.lineTo(x, syMax);
    }
    for (let y = syMin; y <= syMax; y += SUB_CELL) {
      if (y % CELL === 0) continue;
      grid.moveTo(sxMin, y);
      grid.lineTo(sxMax, y);
    }
    grid.stroke();
  }

  // --- Major grid lines (CELL spacing) ---
  const majorPx = CELL * scale;
  if (majorPx >= MAJOR_HIDE_THRESHOLD) {
    const cxMin = Math.floor(wx0 / CELL) * CELL;
    const cxMax = Math.ceil(wx1 / CELL) * CELL;
    const cyMin = Math.floor(wy0 / CELL) * CELL;
    const cyMax = Math.ceil(wy1 / CELL) * CELL;

    grid.setStrokeStyle({ width: 1, color: MAJOR_LINE_COLOR, alpha: MAJOR_LINE_ALPHA });
    for (let x = cxMin; x <= cxMax; x += CELL) {
      grid.moveTo(x, cyMin);
      grid.lineTo(x, cyMax);
    }
    for (let y = cyMin; y <= cyMax; y += CELL) {
      grid.moveTo(cxMin, y);
      grid.lineTo(cxMax, y);
    }
    grid.stroke();

    // --- Dots at CELL intersections ---
    for (let x = cxMin; x <= cxMax; x += CELL) {
      for (let y = cyMin; y <= cyMax; y += CELL) {
        grid.circle(x, y, GRID_DOT_RADIUS);
      }
    }
    grid.fill({ color: GRID_DOT_COLOR, alpha: GRID_DOT_ALPHA });
  }
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
  const patchTypes = usePatchStore((s) => s.patchTypes);
  const frames = useGameStore((s) => s.frames);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
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
  const createConnection = useGameStore((s) => s.createConnection);
  const removeConnection = useGameStore((s) => s.removeConnection);
  const connectionLayerRef = useRef<Graphics | null>(null);
  const wiringLineRef = useRef<Graphics | null>(null);
  const connectionsRef = useRef(connections);
  connectionsRef.current = connections;
  const placedFramesRef = useRef<PlacedFrame[]>([]);
  const wiringRef = useRef<{ sourceId: number; type: "POWER" | "DATA" | "CONVEYOR" } | null>(null);
  const rpcClientRef = useRef(rpcClient);
  rpcClientRef.current = rpcClient;
  const createConnectionRef = useRef(createConnection);
  createConnectionRef.current = createConnection;
  const [wiringMode, setWiringMode] = useState<{ sourceId: number; type: "POWER" | "DATA" | "CONVEYOR" } | null>(null);
  const [frameContextMenu, setFrameContextMenu] = useState<{ x: number; y: number; frameId: number } | null>(null);
  const [frameRenaming, setFrameRenaming] = useState<{ frameId: number; name: string } | null>(null);
  const [compContextMenu, setCompContextMenu] = useState<{
    x: number; y: number; frameId: number; componentId: number;
  } | null>(null);
  const [componentPickerOpen, setComponentPickerOpen] = useState<{ frameId: number } | null>(null);
  const [blueprintPickerOpen, setBlueprintPickerOpen] = useState<{ wx: number; wy: number } | null>(null);
  const [patchInspector, setPatchInspector] = useState<{ patch: Patch; x: number; y: number } | null>(null);
  const drawConnectionsRef = useRef<() => void>(() => {});

  // --- Frame positions map for connection drawing and drag ---
  const framePositionsRef = useRef<Map<number, { x: number; y: number }>>(new Map());

  const getPatchAtWorldPos = (wx: number, wy: number): Patch | null => {
    const gridX = Math.floor(wx / SUB_CELL);
    const gridY = Math.floor(wy / SUB_CELL);
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

    const TYPE_ORDER: Array<"POWER" | "DATA" | "CONVEYOR"> = ["POWER", "DATA", "CONVEYOR"];
    const OFFSET_STEP = 6;

    for (const [, conns] of groups) {
      const sample = conns[0]!;
      const srcPos = framePositionsRef.current.get(sample.source);
      const tgtPos = framePositionsRef.current.get(sample.target);
      const srcFrame = framesRef.current.find((f) => f.id === sample.source);
      const tgtFrame = framesRef.current.find((f) => f.id === sample.target);
      if (!srcPos || !tgtPos || !srcFrame || !tgtFrame) continue;

      const srcCells = FRAME_CELL_SIZES[srcFrame.size] ?? 1;
      const tgtCells = FRAME_CELL_SIZES[tgtFrame.size] ?? 1;
      const baseX1 = srcPos.x + (srcCells * CELL) / 2;
      const baseY1 = srcPos.y + (srcCells * CELL) / 2;
      const baseX2 = tgtPos.x + (tgtCells * CELL) / 2;
      const baseY2 = tgtPos.y + (tgtCells * CELL) / 2;

      const dx = baseX2 - baseX1;
      const dy = baseY2 - baseY1;
      const len = Math.sqrt(dx * dx + dy * dy) || 1;
      const nx = -dy / len;
      const ny = dx / len;

      const presentTypes = TYPE_ORDER.filter((t) => conns.some((c) => c.type === t));
      const count = presentTypes.length;
      const startIndex = -(count - 1) / 2;

      presentTypes.forEach((type, idx) => {
        const offset = (startIndex + idx) * OFFSET_STEP;
        const ox = nx * offset;
        const oy = ny * offset;

        const x1 = baseX1 + ox;
        const y1 = baseY1 + oy;
        const x2 = baseX2 + ox;
        const y2 = baseY2 + oy;

        const color = CONN_COLOR[type] ?? 0xffffff;
        connLayer.setStrokeStyle({ width: 2, color, alpha: 0.7 });
        connLayer.moveTo(x1, y1);
        connLayer.lineTo(x2, y2);
        connLayer.stroke();

        const mx = (x1 + x2) / 2;
        const my = (y1 + y2) / 2;
        connLayer.circle(mx, my, 4);
        connLayer.fill({ color, alpha: 0.9 });
      });
    }
  };
  drawConnectionsRef.current = drawConnections;

  const updateMarkers = useCallback(() => {
    const layer = markerLayerRef.current;
    if (!layer) return;
    layer.clear();
    const currentMarkers = markersRef.current;
    for (const marker of currentMarkers) {
      const colorNum = marker.color ? parseInt(marker.color.replace("#", "0x")) : 0xff0000;
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
        gfx.rect(cx * SUB_CELL, cy * SUB_CELL, SUB_CELL, SUB_CELL);
      }
      gfx.fill({ color, alpha });

      layer.addChild(gfx);
    }
  }, [patches]);

  useEffect(() => {
    renderPatches();
  }, [renderPatches]);

  const parseBlueprintSize = (source: string): string => {
    const match = source.match(/FrameSize\.([SMLG])/);
    return match?.[1] ?? "M";
  };

  useEffect(() => {
    if (!blueprintSupported) return;
    void (async () => {
      try {
        const result = await rpcClient.call<{ blueprints: Record<string, string> }>("blueprint.palette");
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
    const gridX = Math.floor(wx / SUB_CELL);
    const gridY = Math.floor(wy / SUB_CELL);
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
      const x = frame.position?.x ?? (index % 6) * CELL + SUB_CELL;
      const y = frame.position?.y ?? Math.floor(index / 6) * CELL + SUB_CELL;
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
    drawConnections,
    moveFrame,
    selectFrame,
    onFrameMiniInspect,
  });
  dragState.rpcClientRef.current = rpcClient;

  // Keep framePositionsRef in sync with placedFrames
  // Skip frames that are currently being dragged or have pending move RPCs
  useMemo(() => {
    for (const frame of placedFrames) {
      const isDragging = dragState.draggingFrameIdRef.current === frame.id;
      const isPending = dragState.pendingMoveIds.current.has(frame.id);
      if (!isDragging && !isPending) {
        framePositionsRef.current.set(frame.id, { x: frame._x, y: frame._y });
      }
    }
    // Clean up removed frames
    const activeIds = new Set(placedFrames.map((f) => f.id));
    for (const id of framePositionsRef.current.keys()) {
      if (!activeIds.has(id)) {
        framePositionsRef.current.delete(id);
      }
    }
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
        const hasConn = tgtFrame ? frameHasConnector(tgtFrame, wiring.type) : false;
        const hasReq = tgtFrame ? frameMeetsConnectionRequirements(tgtFrame, wiring.type) : false;
        const cardOk = isCardinallySatisfied(currentConns, currentFrames, frameId, wiring.type);
        const pairOk = !hasConnectionBetween(currentConns, wiring.sourceId, frameId, wiring.type);

        if (
          frameId !== wiring.sourceId &&
          tgtFrame &&
          hasConn &&
          hasReq &&
          cardOk &&
          pairOk
        ) {
          createConnectionRef.current(rpcClientRef.current, wiring.sourceId, frameId, wiring.type).catch(console.error);
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
        const hasConn = tgtFrame ? frameHasConnector(tgtFrame, wiring.type) : false;
        const hasReq = tgtFrame ? frameMeetsConnectionRequirements(tgtFrame, wiring.type) : false;
        const cardOk = isCardinallySatisfied(currentConns, currentFrames, frameId, wiring.type);
        if (tgtFrame && hasConn && hasReq && cardOk) {
          createConnectionRef.current(rpcClientRef.current, wiring.sourceId, frameId, wiring.type).catch(console.error);
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
          setContextMenu(null);
          return;
        }
      }

      setFrameContextMenu({ x: e.clientX, y: e.clientY, frameId });
      setCompContextMenu(null);
      setContextMenu(null);
    },
    [],
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
    selectFrame(null);
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

        // No more Pixi frameLayer — frames are rendered as HTML overlays

        redrawGrid(grid, worldContainer, app.screen.width, app.screen.height);

        // Initial overlay sync
        syncOverlayTransform();

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
            const sizeKey = src.match(/FrameSize\.([SMLG])/)?.[1] ?? "M";
            const cells = FRAME_CELL_SIZES[sizeKey] ?? 2;
            const boxSize = cells * CELL;
            ghost.clear();
            ghost.beginFill(0x3b82f6);
            ghost.roundRect(0, 0, boxSize, boxSize, 10);
            ghost.fill();
            ghost.x = wx;
            ghost.y = wy;
            ghost.visible = true;
          } else if (ghost.visible) {
            ghost.visible = false;
          }

          // Update wiring preview line
          const wiring = wiringRef.current;
          if (wiring) {
            const sourceFrame = placedFramesRef.current.find((f) => f.id === wiring.sourceId);
            if (sourceFrame) {
              const srcCells = FRAME_CELL_SIZES[sourceFrame.size] ?? 1;
              const boxSize = srcCells * CELL;
              const cx = sourceFrame._x + boxSize / 2;
              const cy = sourceFrame._y + boxSize / 2;
              const color = CONN_COLOR[wiring.type] ?? 0xffffff;
              wiringLine.clear();
              wiringLine.setStrokeStyle({ width: 2, color, alpha: 0.8 });
              wiringLine.moveTo(cx, cy);
              wiringLine.lineTo(cursorWx, cursorWy);
              wiringLine.stroke();
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
      patchLayerRef.current = null;
      markerLayerRef.current = null;
      connectionLayerRef.current = null;
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
  }, [connections]); // eslint-disable-line react-hooks/exhaustive-deps

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

  return (
    <div ref={hostRef} style={{ width: "100%", height: "100%", position: "relative" }}>
      {/* HTML frame overlay — synced with Pixi worldContainer transform */}
      <FrameOverlay
        ref={overlayRef}
        placedFrames={placedFrames}
        selectedFrameId={selectedFrameId}
        wiringMode={wiringMode}
        connections={connections}
        frames={placedFrames}
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
        const eligibleTargets = frames.filter((frame) => {
          if (frame.id === sourceId) return false;
          if (!frameHasConnector(frame, wiringMode.type)) return false;
          if (!frameMeetsConnectionRequirements(frame, wiringMode.type)) return false;
          if (!isCardinallySatisfied(conns, frames, frame.id, wiringMode.type)) return false;
          if (hasConnectionBetween(conns, sourceId, frame.id, wiringMode.type)) return false;
          return true;
        });
        if (eligibleTargets.length > 0) return null;

        let reason = "No eligible target frames for this connection.";
        if (wiringMode.type === "CONVEYOR") {
          reason = "Conveyor connections require a frame with a Storage component.";
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
      {frameContextMenu && (() => {
        const frame = frames.find((f) => f.id === frameContextMenu.frameId);
        const allConnInfo = (["POWER", "DATA", "CONVEYOR"] as const).map((type) => {
          if (!frame) {
            return { type, available: false as const, reason: "Frame not found." };
          }
          if (!frameHasConnector(frame, type)) {
            return { type, available: false as const, reason: "No connector component on this frame." };
          }
          if (!frameMeetsConnectionRequirements(frame, type)) {
            let reason = "Frame does not meet connection requirements.";
            if (type === "CONVEYOR") {
              reason = "Requires a Storage component on this frame.";
            } else if (type === "DATA") {
              reason = "Requires a Core component on this frame.";
            }
            return { type, available: false as const, reason };
          }
          if (!isCardinallySatisfied(connections, frames, frame.id, type)) {
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
                  .filter((c) => !c.available && c.reason && frame && frameHasConnector(frame, c.type))
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
                    wiringRef.current = { sourceId: frameContextMenu.frameId, type };
                    setWiringMode({ sourceId: frameContextMenu.frameId, type });
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
