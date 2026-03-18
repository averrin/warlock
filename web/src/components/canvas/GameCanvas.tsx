import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import { createPortal } from "react-dom";
import { Application, Assets, Container, Graphics, Sprite, Text, Texture } from "pixi.js";
import type { FederatedPointerEvent } from "pixi.js";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { usePatchStore } from "../../stores/patches";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import type { ComponentDTO } from "../../rpc/types";
import { ComponentContextMenu } from "./ComponentContextMenu";
import { ComponentPicker, BlueprintPicker } from "../picker";

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
const TEXT_RESOLUTION = 2 * (typeof window !== "undefined" ? window.devicePixelRatio : 1);

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

// Component badge strip (matches FrameMiniInspector STATE_COLORS)
const COMP_BADGE_SIZE = 14;
const COMP_BADGE_GAP = 3;
const COMP_BADGE_RADIUS = 3;
const COMP_BADGE_FONT_SIZE = 8;
const COMP_STATE_COLORS: Record<string, number> = {
  ACTIVE: 0x14532d,
  ACTIVATING: 0x3f6212,
  DEACTIVATING: 0x78350f,
  DEACTIVATED: 0x1f2937,
  COMP_ERROR: 0x7f1d1d,
  DESTROYED: 0x450a0a,
  BLOCKED: 0x4a1d96,
  BROKEN: 0x7f1d1d,
};
const COMP_DEFAULT_COLOR = 0x1f2937;
const COMP_ICON_PAD = 2; // padding inside badge for the icon sprite

// Cache loaded icon textures so we don't reload per frame per render
const iconTextureCache = new Map<string, Texture | null>();
const iconTextureLoading = new Set<string>();
const iconTextureListeners = new Set<() => void>();

function notifyIconTextureListeners() {
  for (const cb of iconTextureListeners) {
    try {
      cb();
    } catch {
      // ignore listener errors
    }
  }
}

function subscribeIconTextureUpdates(cb: () => void): () => void {
  iconTextureListeners.add(cb);
  return () => {
    iconTextureListeners.delete(cb);
  };
}

function getIconTexture(iconFilename: string): Texture | null {
  if (iconTextureCache.has(iconFilename)) return iconTextureCache.get(iconFilename)!;
  if (iconTextureLoading.has(iconFilename)) return null; // still loading
  iconTextureLoading.add(iconFilename);
  const url = `/icons/${iconFilename}`;
  void Assets.load<Texture>(url)
    .then((tex) => {
      iconTextureCache.set(iconFilename, tex);
      iconTextureLoading.delete(iconFilename);
      notifyIconTextureListeners();
    })
    .catch(() => {
      iconTextureCache.set(iconFilename, null);
      iconTextureLoading.delete(iconFilename);
      notifyIconTextureListeners();
    });
  return null; // will be available next render cycle
}

type FrameNode = Container & {
  __box?: Graphics;
  __label?: Text;
  __idLabel?: Text;
  __highlight?: Graphics;
  __errorMarker?: Graphics;
  __compBadgeContainer?: Container;
  __compBadgeBg?: Graphics;
  __compBadgeTexts?: Text[];
  __compBadgeSprites?: Sprite[];
};

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
    const mxMin = Math.floor(wx0 / CELL) * CELL;
    const mxMax = Math.ceil(wx1 / CELL) * CELL;
    const myMin = Math.floor(wy0 / CELL) * CELL;
    const myMax = Math.ceil(wy1 / CELL) * CELL;

    grid.setStrokeStyle({ width: 1, color: MAJOR_LINE_COLOR, alpha: MAJOR_LINE_ALPHA });
    for (let x = mxMin; x <= mxMax; x += CELL) {
      grid.moveTo(x, myMin);
      grid.lineTo(x, myMax);
    }
    for (let y = myMin; y <= myMax; y += CELL) {
      grid.moveTo(mxMin, y);
      grid.lineTo(mxMax, y);
    }
    grid.stroke();

    // --- Dots at major intersections ---
    for (let x = mxMin; x <= mxMax; x += CELL) {
      for (let y = myMin; y <= myMax; y += CELL) {
        grid.circle(x, y, GRID_DOT_RADIUS / scale);
      }
    }
    grid.fill({ color: GRID_DOT_COLOR, alpha: GRID_DOT_ALPHA });
  }
}

function updateTextResolution(node: FrameNode, scale: number) {
  const res = Math.max(TEXT_RESOLUTION, Math.ceil(scale * 2));
  if (node.__label && node.__label.resolution !== res) {
    node.__label.resolution = res;
  }
  if (node.__idLabel && node.__idLabel.resolution !== res) {
    node.__idLabel.resolution = res;
  }
  if (node.__compBadgeTexts) {
    for (const bt of node.__compBadgeTexts) {
      if (bt.resolution !== res) bt.resolution = res;
    }
  }
}

function getEventClientXY(e: FederatedPointerEvent): { x: number; y: number } {
  // Pixi v7 FederatedPointerEvent exposes `client` in viewport coordinates.
  const client = (e as any).client as { x?: number; y?: number } | undefined;
  if (typeof client?.x === "number" && typeof client?.y === "number") {
    return { x: client.x, y: client.y };
  }
  const oe = ((e as any).originalEvent ?? (e as any).nativeEvent ?? (e as any).data?.originalEvent) as
    | { clientX?: number; clientY?: number }
    | undefined;
  return { x: oe?.clientX ?? 0, y: oe?.clientY ?? 0 };
}

export function GameCanvas({ rpcClient, onFrameMiniInspect }: Props) {
  const hostRef = useRef<HTMLDivElement | null>(null);
  const appRef = useRef<Application | null>(null);
  const worldRef = useRef<Container | null>(null);
  const gridRef = useRef<Graphics | null>(null);
  const frameLayerRef = useRef<Container | null>(null);
  const patchLayerRef = useRef<Container | null>(null);
  const frameNodeByIdRef = useRef<Map<number, Container>>(new Map());
  const patches = usePatchStore((s) => s.patches);
  const frames = useGameStore((s) => s.frames);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
  const selectFrame = useGameStore((s) => s.selectFrame);
  const moveFrame = useGameStore((s) => s.moveFrame);
  const createFromBlueprint = useGameStore((s) => s.createFromBlueprint);
  const updateFrameMetadata = useGameStore((s) => s.updateFrameMetadata);
  const draggingFrameIdRef = useRef<number | null>(null);
  const dragMovedRef = useRef(false);
  const pendingDropRef = useRef<{ id: number; x: number; y: number } | null>(null);
  const pendingMoveIds = useRef<Set<number>>(new Set());
  const panningRef = useRef(false);
  const panStartRef = useRef({ sx: 0, sy: 0, wx: 0, wy: 0 });
  const framesRef = useRef(frames);
  framesRef.current = frames;
  const ghostRef = useRef<Graphics | null>(null);
  const creatingRef = useRef<string | null>(null);
  const [iconVersion, setIconVersion] = useState(0);

  useEffect(() => {
    return subscribeIconTextureUpdates(() => {
      setIconVersion((v) => v + 1);
    });
  }, []);

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
  const placedFramesRef = useRef<ReturnType<typeof Array.prototype.map>>([]);
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
  const drawConnectionsRef = useRef<() => void>(() => {});

  // Click-outside handler for all context menus
  useEffect(() => {
    const handleClickOutside = (e: MouseEvent) => {
      const target = e.target as HTMLElement;
      if (!target.closest("[data-context-menu]")) {
        setContextMenu(null);
        setFrameContextMenu(null);
        setCompContextMenu(null);
        setFrameRenaming(null);
      }
    };
    document.addEventListener("mousedown", handleClickOutside);
    return () => document.removeEventListener("mousedown", handleClickOutside);
  }, []);

  const drawConnections = () => {
    const connLayer = connectionLayerRef.current;
    if (!connLayer) return;
    connLayer.clear();

    // Group connections by unordered frame pair so we can draw
    // a bundle of parallel lines instead of overlapping ones.
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
    const OFFSET_STEP = 6; // pixels between parallel lines

    for (const [, conns] of groups) {
      // Use the first connection to find endpoints; all share same frames.
      const sample = conns[0]!;
      const srcNode = frameNodeByIdRef.current.get(sample.source);
      const tgtNode = frameNodeByIdRef.current.get(sample.target);
      const srcFrame = framesRef.current.find((f) => f.id === sample.source);
      const tgtFrame = framesRef.current.find((f) => f.id === sample.target);
      if (!srcNode || !tgtNode || !srcFrame || !tgtFrame) continue;

      const srcCells = FRAME_CELL_SIZES[srcFrame.size] ?? 1;
      const tgtCells = FRAME_CELL_SIZES[tgtFrame.size] ?? 1;
      const baseX1 = srcNode.x + (srcCells * CELL) / 2;
      const baseY1 = srcNode.y + (srcCells * CELL) / 2;
      const baseX2 = tgtNode.x + (tgtCells * CELL) / 2;
      const baseY2 = tgtNode.y + (tgtCells * CELL) / 2;

      const dx = baseX2 - baseX1;
      const dy = baseY2 - baseY1;
      const len = Math.sqrt(dx * dx + dy * dy) || 1;
      const nx = -dy / len;
      const ny = dx / len;

      // Determine which types exist for this pair in a stable order
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
    const m = source.match(/FrameSize\.([SMLG])/);
    return m ? m[1] : "M";
  };

  useEffect(() => {
    if (!blueprintSupported) return;
    void rpcClient
      .call<{ blueprints: Record<string, string> }>("code.blueprints")
      .then((data) => {
        const map = data.blueprints ?? {};
        blueprintMapRef.current = map;
        setBlueprintMap(map);
      })
      .catch(() => setBlueprintMap({}));
  }, [rpcClient, blueprintSupported]);


  const enterCreationMode = useCallback((blueprint: string) => {
    setCreationBlueprint(blueprint);
    creatingRef.current = blueprint;
    setContextMenu(null);
    setCompContextMenu(null);
    // Pre-draw ghost at correct size immediately
    const ghost = ghostRef.current;
    if (ghost) {
      const src = blueprintMapRef.current[blueprint] ?? "";
      const size = parseBlueprintSize(src);
      const cells = FRAME_CELL_SIZES[size] ?? 2;
      const boxSize = cells * CELL;
      ghost.clear();
      ghost.beginFill(0x3b82f6);
      ghost.roundRect(0, 0, boxSize, boxSize, 10);
      ghost.fill();
    }
  }, []);

  const exitCreationMode = useCallback(() => {
    setCreationBlueprint(null);
    creatingRef.current = null;
    const ghost = ghostRef.current;
    if (ghost) ghost.visible = false;
  }, []);

  const placedFrames = useMemo(() => {
    return frames.map((frame, index) => {
      const x = frame.position?.x ?? (index % 6) * CELL + SUB_CELL;
      const y = frame.position?.y ?? Math.floor(index / 6) * CELL + SUB_CELL;
      return { ...frame, _x: x, _y: y };
    });
  }, [frames]);
  placedFramesRef.current = placedFrames;

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

  const onFrameMove = (event: FederatedPointerEvent) => {
    if (draggingFrameIdRef.current === null || !frameLayerRef.current) {
      return;
    }
    const id = draggingFrameIdRef.current;
    const local = event.getLocalPosition(frameLayerRef.current);
    const x = Math.round(local.x / CELL) * CELL;
    const y = Math.round(local.y / CELL) * CELL;
    const draggedFrame = framesRef.current.find((f) => f.id === id);
    const size = draggedFrame?.size ?? "S";
    if (isOverlapping(x, y, size, id)) {
      return;
    }
    dragMovedRef.current = true;
    pendingDropRef.current = { id, x, y };
    const node = frameNodeByIdRef.current.get(id);
    if (node) {
      node.x = x;
      node.y = y;
    }
    drawConnectionsRef.current();
  };

  const onFrameUp = () => {
    const drop = pendingDropRef.current;
    draggingFrameIdRef.current = null;
    const moved = dragMovedRef.current;
    dragMovedRef.current = false;
    pendingDropRef.current = null;
    if (!drop || !moved) {
      return;
    }
    pendingMoveIds.current.add(drop.id);
    moveFrame(rpcClient, drop.id, drop.x, drop.y).finally(() => {
      pendingMoveIds.current.delete(drop.id);
    });
  };

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
    if (creatingRef.current && frameLayerRef.current) {
      const bp = creatingRef.current;
      const local = event.getLocalPosition(frameLayerRef.current);
      const x = Math.round(local.x / CELL) * CELL;
      const y = Math.round(local.y / CELL) * CELL;
      exitCreationMode();
      void createFromBlueprint(rpcClient, bp, { x, y });
      return;
    }
    selectFrame(null);
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
        patchLayerRef.current = patchLayer;

        const connectionLayer = new Graphics();
        connectionLayerRef.current = connectionLayer;
        worldContainer.addChild(connectionLayer);

        const frameLayer = new Container();
        frameLayerRef.current = frameLayer;
        frameLayer.eventMode = "static";
        worldContainer.addChild(frameLayer);

        // Register drag listeners on stage so they fire even when
        // the pointer moves outside the frameLayer bounds (up/left drag).
        app.stage.on("pointermove", onFrameMove);
        app.stage.on("pointerup", onFrameUp);
        app.stage.on("pointerupoutside", onFrameUp);

        redrawGrid(grid, worldContainer, app.screen.width, app.screen.height);

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

          // Update text resolution for all frame nodes on zoom
          for (const node of frameNodeByIdRef.current.values()) {
            updateTextResolution(node as FrameNode, newScale);
          }
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

        // --- Right-click context menu ---
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

          // Detect frame hit for contextual menu / wiring
          let hitFrameId: number | null = null;
          for (const frame of placedFramesRef.current as { id: number; size: string; _x: number; _y: number }[]) {
            const cells = FRAME_CELL_SIZES[frame.size] ?? 1;
            const boxSize = cells * CELL;
            if (rawWx >= frame._x && rawWx <= frame._x + boxSize &&
                rawWy >= frame._y && rawWy <= frame._y + boxSize) {
              hitFrameId = frame.id;
              break;
            }
          }

          // If we are in wiring mode and right-click a valid target frame, complete the connection immediately
          const wiring = wiringRef.current;
          if (wiring && hitFrameId !== null && hitFrameId !== wiring.sourceId) {
            const currentFrames = framesRef.current;
            const currentConns = connectionsRef.current;
            const tgtFrame = currentFrames.find((f) => f.id === hitFrameId);
            const hasConn = tgtFrame ? frameHasConnector(tgtFrame, wiring.type) : false;
            const hasReq = tgtFrame ? frameMeetsConnectionRequirements(tgtFrame, wiring.type) : false;
            const cardOk = isCardinallySatisfied(currentConns, currentFrames, hitFrameId, wiring.type);
            console.log(
              "[wiring] contextmenu frame",
              hitFrameId,
              "source",
              wiring.sourceId,
              "type",
              wiring.type,
              "hasConn",
              hasConn,
              "hasReq",
              hasReq,
              "cardOk",
              cardOk,
            );
            if (tgtFrame && hasConn && hasReq && cardOk) {
              createConnectionRef.current(rpcClientRef.current, wiring.sourceId, hitFrameId, wiring.type).catch(console.error);
            }
            wiringRef.current = null;
            setWiringMode(null);
            const wl = wiringLineRef.current;
            if (wl) wl.visible = false;
            return;
          }

          if (hitFrameId !== null) {
            // Check if click hit a component badge
            const hitFrame = (placedFramesRef.current as { id: number; size: string; _x: number; _y: number }[])
              .find((f) => f.id === hitFrameId);
            const frameDTO = framesRef.current.find((f) => f.id === hitFrameId);
            const components = frameDTO?.components ?? [];
            let hitCompId: number | null = null;

            // Use pre-computed hit areas stored on the Pixi node during render
            const frameNode = frameNodeByIdRef.current.get(hitFrameId);
            const hitAreas = (frameNode as any)?.__compHitAreas as
              { componentId: number; wx: number; wy: number; w: number; h: number }[] | undefined;
            if (hitAreas) {
              const hitPad = 5;
              for (const area of hitAreas) {
                if (
                  rawWx >= area.wx - hitPad && rawWx <= area.wx + area.w + hitPad &&
                  rawWy >= area.wy - hitPad && rawWy <= area.wy + area.h + hitPad
                ) {
                  hitCompId = area.componentId;
                  break;
                }
              }
            }

            if (hitCompId !== null) {
              setCompContextMenu({ x: e.clientX, y: e.clientY, frameId: hitFrameId, componentId: hitCompId });
              setFrameContextMenu(null);
              setContextMenu(null);
            } else {
              setFrameContextMenu({ x: sx, y: sy, frameId: hitFrameId });
              setCompContextMenu(null);
              setContextMenu(null);
            }
          } else {
            setContextMenu({ x: sx, y: sy, wx, wy });
            setFrameContextMenu(null);
            setCompContextMenu(null);
          }
        };
        canvasEl.addEventListener("contextmenu", onContextMenu);

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
            const sourceFrame = (placedFramesRef.current as { id: number; size: string; _x: number; _y: number }[])
              .find((f) => f.id === wiring.sourceId);
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
          window.removeEventListener("keydown", onCreationKeydown);
        };
      });

    return () => {
      disposed = true;
      appRef.current = null;
      worldRef.current = null;
      gridRef.current = null;
      patchLayerRef.current = null;
      connectionLayerRef.current = null;
      wiringLineRef.current = null;
      frameLayerRef.current = null;
      frameNodeByIdRef.current.clear();
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

  useEffect(() => {
    const frameLayer = frameLayerRef.current;
    const app = appRef.current;
    if (!frameLayer || !app) {
      return;
    }

    const activeIds = new Set<number>();
    for (const frame of placedFrames) {
      activeIds.add(frame.id);
      let node = frameNodeByIdRef.current.get(frame.id) as FrameNode | undefined;
      if (!node) {
        const frameId = frame.id;
        node = new Container() as FrameNode;
        node.eventMode = "static";
        node.on("pointerdown", (e: FederatedPointerEvent) => {
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
          console.log(
            "[wiring] clicked frame",
            frameId,
            "source",
            wiring.sourceId,
            "type",
            wiring.type,
            "hasConn",
            hasConn,
            "hasReq",
            hasReq,
            "cardOk",
            cardOk,
            "pairOk",
            pairOk,
          );
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
          if (e.button !== 0) return;
          e.stopPropagation();
          draggingFrameIdRef.current = frameId;
          dragMovedRef.current = false;
          pendingDropRef.current = null;
          const { x: clientX, y: clientY } = getEventClientXY(e);
          onFrameMiniInspect?.(frameId, clientX, clientY);
          selectFrame(frameId);
        });

        const box = new Graphics();
        node.__box = box;
        node.addChild(box);

        const highlight = new Graphics();
        node.__highlight = highlight;
        node.addChild(highlight);

        const errorMarker = new Graphics();
        node.__errorMarker = errorMarker;
        node.addChild(errorMarker);

        const label = new Text({
          text: "",
          resolution: TEXT_RESOLUTION,
          style: {
            fill: 0xf8fafc,
            fontSize: 12,
          },
        });
        label.x = 8;
        label.y = 8;
        node.__label = label;
        node.addChild(label);

        const idLabel = new Text({
          text: "",
          resolution: TEXT_RESOLUTION,
          style: {
            fill: 0xcbd5e1,
            fontSize: 11,
          },
        });
        idLabel.x = 8;
        idLabel.y = 28;
        node.__idLabel = idLabel;
        node.addChild(idLabel);

        // Component badge strip (positioned at bottom of frame box)
        const compBadgeContainer = new Container();
        const compBadgeBg = new Graphics();
        compBadgeContainer.addChild(compBadgeBg);
        node.__compBadgeContainer = compBadgeContainer;
        node.__compBadgeBg = compBadgeBg;
        node.__compBadgeTexts = [];
        node.addChild(compBadgeContainer);

        const worldScale = worldRef.current?.scale.x ?? 1;
        updateTextResolution(node, worldScale);

        frameNodeByIdRef.current.set(frame.id, node);
        frameLayer.addChild(node);
      }

      if (draggingFrameIdRef.current !== frame.id && !pendingMoveIds.current.has(frame.id)) {
        node.x = frame._x;
        node.y = frame._y;
      }
      const cells = FRAME_CELL_SIZES[frame.size] ?? 1;
      const boxSize = cells * CELL;
      const isSelected = selectedFrameId === frame.id;

      let isWiringEligible = false;
      let isWiringSource = false;
      if (wiringMode) {
        isWiringSource = frame.id === wiringMode.sourceId;
        if (!isWiringSource) {
          const conns = connectionsRef.current;
          isWiringEligible =
            frameHasConnector(frame, wiringMode.type) &&
            frameMeetsConnectionRequirements(frame, wiringMode.type) &&
            isCardinallySatisfied(conns, framesRef.current, frame.id, wiringMode.type) &&
            !hasConnectionBetween(conns, wiringMode.sourceId, frame.id, wiringMode.type);
        }
      }

      const prevTag = (node as any).__drawTag as string | undefined;
      const nextTag = `${frame.size}:${isSelected}:${wiringMode ? wiringMode.type + (isWiringSource ? "S" : isWiringEligible ? "E" : "N") : ""}`;
      if (prevTag !== nextTag) {
        (node as any).__drawTag = nextTag;
        node.alpha = wiringMode && !isWiringSource && !isWiringEligible ? 0.35 : 1;
        if (node.__box) {
          node.__box.clear();
          node.__box.beginFill(isSelected ? 0x2563eb : 0x334155);
          node.__box.roundRect(0, 0, boxSize, boxSize, 10);
          node.__box.fill();
        }
        if (node.__highlight) {
          node.__highlight.clear();
          if (wiringMode) {
            const color = CONN_COLOR[wiringMode.type] ?? 0xffffff;
            if (isWiringSource) {
              node.__highlight.setStrokeStyle({ width: 3, color, alpha: 1 });
              node.__highlight.roundRect(-2, -2, boxSize + 4, boxSize + 4, 12);
              node.__highlight.stroke();
            } else if (isWiringEligible) {
              node.__highlight.setStrokeStyle({ width: 2, color, alpha: 0.85 });
              node.__highlight.roundRect(-2, -2, boxSize + 4, boxSize + 4, 12);
              node.__highlight.stroke();
            }
          }
        }
      }
      if (node.__label) {
        node.__label.text = `${frame.name} #${frame.id}`;
      }

      if (node.__errorMarker) {
        const hasComponentError =
          (frame.components ?? []).some((c) => (c as ComponentDTO).error && (c as ComponentDTO).error !== "") ||
          frame.canvas_badges?.has_error;
        node.__errorMarker.clear();
        if (hasComponentError) {
          const markerSize = 10;
          const padding = 4;
          node.__errorMarker.beginFill(0x7f1d1d);
          node.__errorMarker.drawCircle(boxSize - padding - markerSize / 2, padding + markerSize / 2, markerSize / 2);
          node.__errorMarker.endFill();
        }
      }

      // Render component badge strip (mini-inspector style)
      if (node.__compBadgeContainer && node.__compBadgeBg) {
        const components: ComponentDTO[] = frame.components ?? [];
        // Include icon filename + whether its texture is cached in the tag
        // so badges re-render when an icon texture finishes loading
        const storageComp = components.find((c) => c.storage);
        const storageTag = storageComp?.storage
          ? `${storageComp.storage.slots_used ?? 0}/${storageComp.storage.slots_total ?? storageComp.storage.slots_count ?? 0}`
          : "";
        const compTag = components
          .map((c) => {
            const iconFile = (c.metadata?.icon || c.icon || "").trim();
            const hasTex = iconFile ? iconTextureCache.has(iconFile) : false;
            return `${c.id}:${c.state}:${iconFile}:${hasTex}`;
          })
          .join(",") + (storageTag ? `:storage:${storageTag}` : "");
        const prevCompTag = (node as any).__compBadgeTag as string | undefined;
        if (compTag !== prevCompTag) {
          (node as any).__compBadgeTag = compTag;

          // Remove old children
          for (const bt of node.__compBadgeTexts ?? []) {
            node.__compBadgeContainer!.removeChild(bt);
            bt.destroy();
          }
          for (const sp of node.__compBadgeSprites ?? []) {
            node.__compBadgeContainer!.removeChild(sp);
            sp.destroy();
          }
          node.__compBadgeTexts = [];
          node.__compBadgeSprites = [];
          node.__compBadgeBg.clear();

          if (components.length > 0) {
            node.__compBadgeContainer.visible = true;
            const maxRowWidth = Math.max(COMP_BADGE_SIZE, boxSize - 12); // account for container x padding
            let cursorX = 0;
            let cursorY = 0;
            let rowCount = 1;

            const placeChip = (width: number) => {
              if (cursorX > 0 && cursorX + width > maxRowWidth) {
                cursorX = 0;
                cursorY += COMP_BADGE_SIZE + COMP_BADGE_GAP;
                rowCount += 1;
              }
              const x = cursorX;
              const y = cursorY;
              cursorX += width + COMP_BADGE_GAP;
              return { x, y };
            };

            for (const comp of components) {
              const color = COMP_STATE_COLORS[comp.state] ?? COMP_DEFAULT_COLOR;
              const { x: chipX, y: chipY } = placeChip(COMP_BADGE_SIZE);

              // Draw colored square background
              node.__compBadgeBg.beginFill(color, 0.9);
              node.__compBadgeBg.roundRect(chipX, chipY, COMP_BADGE_SIZE, COMP_BADGE_SIZE, COMP_BADGE_RADIUS);
              node.__compBadgeBg.fill();
              // Border
              node.__compBadgeBg.setStrokeStyle({ width: 1, color: 0x334155, alpha: 0.8 });
              node.__compBadgeBg.roundRect(chipX, chipY, COMP_BADGE_SIZE, COMP_BADGE_SIZE, COMP_BADGE_RADIUS);
              node.__compBadgeBg.stroke();

              // Try to render icon sprite; fall back to letter glyph
              const iconFile = (comp.metadata?.icon || comp.icon || "").trim();
              const tex = iconFile ? getIconTexture(iconFile) : null;

              if (tex) {
                const sprite = new Sprite(tex);
                const innerSize = COMP_BADGE_SIZE - COMP_ICON_PAD * 2;
                sprite.width = innerSize;
                sprite.height = innerSize;
                sprite.x = chipX + COMP_ICON_PAD;
                sprite.y = chipY + COMP_ICON_PAD;
                node.__compBadgeContainer.addChild(sprite);
                node.__compBadgeSprites!.push(sprite);
              } else {
                // Fallback: first letter of name
                const label = (iconFile || comp.name || "?").trim();
                const glyph = label.length > 0 ? label[0]!.toUpperCase() : "?";
                const txt = new Text({
                  text: glyph,
                  resolution: TEXT_RESOLUTION,
                  style: { fill: 0xe2e8f0, fontSize: COMP_BADGE_FONT_SIZE, fontWeight: "bold" },
                });
                txt.x = chipX + (COMP_BADGE_SIZE - txt.width) / 2;
                txt.y = chipY + (COMP_BADGE_SIZE - COMP_BADGE_FONT_SIZE) / 2;
                node.__compBadgeContainer.addChild(txt);
                node.__compBadgeTexts!.push(txt);
              }

            }

            // Storage badge chip
            if (storageComp?.storage) {
              const used = storageComp.storage.slots_used ?? storageComp.storage.slots.filter((s: { stack: unknown }) => s.stack).length;
              const total = storageComp.storage.slots_total ?? storageComp.storage.slots_count ?? storageComp.storage.slots.length;
              const storageTxt = new Text({
                text: `📦 ${used}/${total}`,
                resolution: TEXT_RESOLUTION,
                style: { fill: 0xe2e8f0, fontSize: COMP_BADGE_FONT_SIZE },
              });
              const chipWidth = storageTxt.width + 4;
              const { x: chipX, y: chipY } = placeChip(chipWidth);
              storageTxt.x = chipX + 2;
              storageTxt.y = chipY + (COMP_BADGE_SIZE - COMP_BADGE_FONT_SIZE) / 2;
              node.__compBadgeContainer.addChild(storageTxt);
              node.__compBadgeTexts!.push(storageTxt);
            }

            // Position at bottom-left of frame box, accounting for wrapped rows
            node.__compBadgeContainer.x = 6;
            const totalHeight = rowCount * COMP_BADGE_SIZE + (rowCount - 1) * COMP_BADGE_GAP;
            node.__compBadgeContainer.y = boxSize - totalHeight - 6;

            // Store hit areas on the node so the click handler can reuse exact positions
            const containerWorldX = frame._x + 6;
            const containerWorldY = frame._y + boxSize - totalHeight - 6;
            // Re-walk the placeChip positions for components only (reusing same cursorX/Y state would be wrong since storage chip was placed too)
            let hitCursorX = 0;
            let hitCursorY = 0;
            const hitMaxRowWidth = Math.max(COMP_BADGE_SIZE, boxSize - 12);
            const hitAreas: { componentId: number; wx: number; wy: number; w: number; h: number }[] = [];
            for (const comp of components) {
              if (hitCursorX > 0 && hitCursorX + COMP_BADGE_SIZE > hitMaxRowWidth) {
                hitCursorX = 0;
                hitCursorY += COMP_BADGE_SIZE + COMP_BADGE_GAP;
              }
              hitAreas.push({
                componentId: comp.id,
                wx: containerWorldX + hitCursorX,
                wy: containerWorldY + hitCursorY,
                w: COMP_BADGE_SIZE,
                h: COMP_BADGE_SIZE,
              });
              hitCursorX += COMP_BADGE_SIZE + COMP_BADGE_GAP;
            }
            (node as any).__compHitAreas = hitAreas;
          } else {
            node.__compBadgeContainer.visible = false;
          }
        }
      }
    }

    for (const [id, node] of frameNodeByIdRef.current) {
      if (!activeIds.has(id)) {
        frameLayer.removeChild(node);
        node.destroy({ children: true });
        frameNodeByIdRef.current.delete(id);
      }
    }

    app.stage.off("pointerdown");
    app.stage.on("pointerdown", (e: FederatedPointerEvent) => {
      if (e.button !== 0) return;

      // If we're wiring, handle target selection at stage level using hit-testing
      const wiring = wiringRef.current;
      if (wiring) {
        const world = worldRef.current;
        if (!world) return;

        const pos = e.getLocalPosition(world);
        const wx = pos.x;
        const wy = pos.y;

        let hitFrameId: number | null = null;
        for (const frame of placedFramesRef.current as { id: number; size: string; _x: number; _y: number }[]) {
          const cells = FRAME_CELL_SIZES[frame.size] ?? 1;
          const boxSize = cells * CELL;
          if (wx >= frame._x && wx <= frame._x + boxSize && wy >= frame._y && wy <= frame._y + boxSize) {
            hitFrameId = frame.id;
            break;
          }
        }

        if (hitFrameId !== null && hitFrameId !== wiring.sourceId) {
          const currentFrames = framesRef.current;
          const currentConns = connectionsRef.current;
          const tgtFrame = currentFrames.find((f) => f.id === hitFrameId);
          const hasConn = tgtFrame ? frameHasConnector(tgtFrame, wiring.type) : false;
          const hasReq = tgtFrame ? frameMeetsConnectionRequirements(tgtFrame, wiring.type) : false;
          const cardOk = isCardinallySatisfied(currentConns, currentFrames, hitFrameId, wiring.type);
          const pairOk = !hasConnectionBetween(currentConns, wiring.sourceId, hitFrameId, wiring.type);
          console.log(
            "[wiring] stage hit",
            hitFrameId,
            "source",
            wiring.sourceId,
            "type",
            wiring.type,
            "hasConn",
            hasConn,
            "hasReq",
            hasReq,
            "cardOk",
            cardOk,
            "pairOk",
            pairOk,
          );
          if (tgtFrame && hasConn && hasReq && cardOk && pairOk) {
            createConnectionRef.current(rpcClientRef.current, wiring.sourceId, hitFrameId, wiring.type).catch(console.error);
          }
          wiringRef.current = null;
          setWiringMode(null);
          const wl = wiringLineRef.current;
          if (wl) wl.visible = false;
          return;
        }

        // Clicked while wiring but no valid target frame: cancel wiring
        wiringRef.current = null;
        setWiringMode(null);
        const wl = wiringLineRef.current;
        if (wl) wl.visible = false;
        return;
      }

      // Normal background click behavior
      if (e.target === app.stage) {
        void onBackgroundClick(e);
      }
    });

    drawConnectionsRef.current();
  }, [onBackgroundClick, placedFrames, selectFrame, selectedFrameId, wiringMode, iconVersion]);

  return (
    <div ref={hostRef} style={{ width: "100%", height: "100%", position: "relative" }}>
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
    </div>
  );
}
