import { forwardRef, useImperativeHandle, useRef, useCallback, useLayoutEffect, useMemo } from "react";
import { FrameCard, type PlacedFrame, type WiringState } from "./FrameCard";
import type { ConnectionDTO } from "../../rpc/types";
import { CELL, connectionWiringGeometryOk, frameCenterFromTopLeft, FRAME_CELL_SIZES } from "./connectionGeometry";

type ConnectionMedium = "WIRE" | "WIRELESS" | "BEAM";

function getConnectorComponentName(type: string, medium: ConnectionMedium): string | null {
  if (type === "POWER") {
    if (medium === "WIRE") return "Power Wire Connector";
    if (medium === "WIRELESS") return "Power Wireless Emitter"; // Fallback
    return "Power Beam Connector";
  }
  if (type === "DATA") {
    if (medium === "WIRE") return "Data Wire Connector";
    if (medium === "WIRELESS") return "Data Wireless Emitter"; // Fallback
    return "Data Beam Connector";
  }
  if (type === "CONVEYOR") return "Conveyor Connector";
  if (type === "POE") return "PoE Connector";
  return null;
}

function getConnectorNamesForFrame(type: string, medium: ConnectionMedium): string[] {
  if (medium === "WIRELESS") {
    if (type === "POWER") return ["Power Wireless Emitter", "Power Wireless Receiver"];
    if (type === "DATA") return ["Data Wireless Emitter", "Data Wireless Receiver"];
  }
  const primary = getConnectorComponentName(type, medium);
  if (!primary) return [];
  const names = [primary];
  if (type === "DATA" && medium === "WIRE") names.push("Data Relay");
  if (type === "CONVEYOR") names.push("Conveyor Relay");
  return names;
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

function getMaxConnectionDistanceForFrame(
  frame: { components?: any[]; size: string; id: number },
  type: string,
  medium: ConnectionMedium,
): number {
  const names = getConnectorNamesForFrame(type, medium);
  let best = Number.POSITIVE_INFINITY;
  for (const n of names) {
    const connector = (frame.components ?? []).find((c) => c.name === n);
    if (!connector) continue;
    const attr =
      connector?.metadata?.attributes?.max_connection_distance ??
      (connector?.attributes as Record<string, unknown> | undefined)?.max_connection_distance;
    const d = parseFinalNumber(attr);
    if (d != null) best = Math.min(best, d);
  }
  return best;
}

export interface FrameOverlayHandle {
  /** Update all card positions to match Pixi worldContainer. Direct DOM writes — no React re-render. */
  syncTransform(scale: number, tx: number, ty: number): void;
  /** Get the current transform values */
  getTransform(): { scale: number; tx: number; ty: number };
}

interface FrameOverlayProps {
  placedFrames: PlacedFrame[];
  selectedFrameIds: number[];
  wiringMode: { sourceId: number; type: string; medium?: string } | null;
  connections: ConnectionDTO[];
  frames: PlacedFrame[];
  /** Terrain obstacle grid keys for wire/beam geometry (same as GameCanvas drag rules). */
  obstacleCells: Set<string>;
  zoom: number;
  onFramePointerDown: (e: React.PointerEvent, frameId: number) => void;
  onFrameContextMenu: (e: React.MouseEvent, frameId: number) => void;
}

function frameHasConnector(
  frame: { components?: { name: string }[] },
  type: string,
  medium: ConnectionMedium,
): boolean {
  const names = getConnectorNamesForFrame(type, medium);
  return names.some((n) => (frame.components ?? []).some((c) => c.name === n));
}

function frameMeetsConnectionRequirements(
  frame: { components?: { name: string; storage?: unknown }[] },
  type: string,
  _medium: ConnectionMedium,
): boolean {
  if (type === "CONVEYOR") {
    if ((frame.components ?? []).some((c) => c.name === "Conveyor Relay")) return true;
    return (frame.components ?? []).some((c) => Boolean(c.storage));
  }
  if (type === "DATA" || type === "POE") {
    if ((frame.components ?? []).some((c) => c.name === "Data Relay")) return true;
    return (frame.components ?? []).some((c) => c.name === "Core");
  }
  return true;
}

function isCardinallySatisfied(
  conns: ConnectionDTO[],
  frames: { id: number; components?: any[] }[],
  frameId: number,
  type: string,
  medium: ConnectionMedium,
): boolean {
  const frame = frames.find((f) => f.id === frameId);
  if (!frame) return false;
  const nameSet = new Set(getConnectorNamesForFrame(type, medium));
  const def = type === "POWER" ? 10 : 1;
  let maxTotal = 0;
  for (const c of frame.components ?? []) {
    if (!nameSet.has(c.name)) continue;
    const attr =
      (c as any)?.metadata?.attributes?.max_connections ??
      (c.attributes as Record<string, unknown> | undefined)?.max_connections;
    const n = parseFinalNumber(attr);
    maxTotal += n != null ? n : def;
  }
  if (maxTotal <= 0) maxTotal = type === "POWER" ? 10 : 1;
  const count = conns.filter(
    (c) =>
      (c.source === frameId || c.target === frameId) &&
      c.type === type &&
      (c.medium ?? "WIRE") === medium,
  ).length;
  return count < maxTotal;
}

function hasConnectionBetween(
  conns: ConnectionDTO[],
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

export const FrameOverlay = forwardRef<FrameOverlayHandle, FrameOverlayProps>(
  function FrameOverlay(
    {
      placedFrames,
      selectedFrameIds,
      wiringMode,
      connections,
      frames,
      obstacleCells,
      zoom,
      onFramePointerDown,
      onFrameContextMenu,
    },
    ref,
  ) {
    const containerRef = useRef<HTMLDivElement>(null);
    const transformRef = useRef({ scale: 1, tx: 0, ty: 0 });
    const selectedSet = useMemo(() => new Set(selectedFrameIds), [selectedFrameIds]);

    /** Lower world Y first in DOM (underneath); higher on screen (smaller Y) last so they receive pointer hits first when stacked. */
    const orderedFrames = useMemo(
      () => [...placedFrames].sort((a, b) => b._y - a._y),
      [placedFrames],
    );

    /**
     * Position each card using CSS zoom for crisp text rendering.
     *
     * CSS zoom scales the element AND its left/top offsets, so we must
     * divide the screen-space position by scale to compensate:
     *   visual left = (screenX / scale) * zoom = screenX  ✓
     *
     * Width/height stay in world units — zoom handles visual scaling.
     */
    const applyScreenPositions = useCallback(() => {
      const container = containerRef.current;
      if (!container) return;
      const { scale, tx, ty } = transformRef.current;
      const cards = container.children;
      for (let i = 0; i < cards.length; i++) {
        const card = cards[i] as HTMLElement;
        const worldX = parseFloat(card.dataset.worldX ?? "0");
        const worldY = parseFloat(card.dataset.worldY ?? "0");
        const worldSize = parseFloat(card.dataset.worldSize ?? "75");
        const screenX = worldX * scale + tx;
        const screenY = worldY * scale + ty;
        // Divide by scale to compensate for CSS zoom multiplying left/top
        card.style.left = `${screenX / scale}px`;
        card.style.top = `${screenY / scale}px`;
        card.style.width = `${worldSize}px`;
        card.style.height = `${worldSize}px`;
        (card.style as any).zoom = scale;
      }
    }, []);

    useImperativeHandle(ref, () => ({
      syncTransform(scale: number, tx: number, ty: number) {
        transformRef.current = { scale, tx, ty };
        applyScreenPositions();
      },
      getTransform() {
        return transformRef.current;
      },
    }));

    const getWiringState = useCallback(
      (frameId: number): WiringState | null => {
        if (!wiringMode) return null;
        const isSource = frameId === wiringMode.sourceId;
        if (isSource) return { type: wiringMode.type, isSource: true, isEligible: false };

        const medium = (wiringMode.medium ?? "WIRE") as ConnectionMedium;
        const sourceFrame = frames.find((f) => f.id === wiringMode.sourceId) ?? null;
        const targetFrame = frames.find((f) => f.id === frameId) ?? null;
        const getPosition = (id: number) => {
          const f = frames.find((x) => x.id === id);
          return f ? { x: f._x, y: f._y } : null;
        };
        const getFrameSize = (id: number) => frames.find((f) => f.id === id)?.size;
        let geometryOk = false;
        if (sourceFrame && targetFrame) {
          const srcOrigin = getPosition(wiringMode.sourceId);
          const tgtOrigin = getPosition(frameId);
          if (srcOrigin && tgtOrigin) {
            const sourceCenter = frameCenterFromTopLeft(srcOrigin, getFrameSize(wiringMode.sourceId));
            const targetCenter = frameCenterFromTopLeft(tgtOrigin, getFrameSize(frameId));
            const centerDist = Math.hypot(targetCenter.x - sourceCenter.x, targetCenter.y - sourceCenter.y);
            const sourceMax = getMaxConnectionDistanceForFrame(sourceFrame, wiringMode.type, medium);
            const targetMax = getMaxConnectionDistanceForFrame(targetFrame, wiringMode.type, medium);
            const effectiveMax = Math.min(sourceMax, targetMax);
            geometryOk = connectionWiringGeometryOk(
              wiringMode.sourceId,
              frameId,
              wiringMode.type,
              medium,
              effectiveMax,
              centerDist,
              frames,
              connections,
              getPosition,
              getFrameSize,
              obstacleCells,
            );
          }
        }

        const isEligible =
          frameHasConnector(
            frames.find((f) => f.id === frameId) ?? { components: [] },
            wiringMode.type,
            medium,
          ) &&
          frameMeetsConnectionRequirements(
            frames.find((f) => f.id === frameId) ?? { components: [] },
            wiringMode.type,
            medium,
          ) &&
          isCardinallySatisfied(
            connections,
            frames,
            frameId,
            wiringMode.type,
            medium,
          ) &&
          geometryOk &&
          !hasConnectionBetween(
            connections,
            wiringMode.sourceId,
            frameId,
            wiringMode.type,
            medium,
          );

        return { type: wiringMode.type, isSource: false, isEligible };
      },
      [wiringMode, connections, frames, obstacleCells],
    );

    // After React renders new/updated cards, reapply screen positions.
    // useLayoutEffect runs synchronously after DOM mutations, preventing flash.
    useLayoutEffect(() => {
      applyScreenPositions();
    });

    // Initial mount — set container ref and kick off first positioning
    const containerRefCallback = useCallback(
      (el: HTMLDivElement | null) => {
        (containerRef as any).current = el;
        if (el) {
          requestAnimationFrame(() => applyScreenPositions());
        }
      },
      [applyScreenPositions],
    );

    return (
      <div
        style={{
          position: "absolute",
          top: 0,
          left: 0,
          width: "100%",
          height: "100%",
          overflow: "hidden",
          pointerEvents: "none",
          zIndex: 10,
        }}
      >
        <div ref={containerRefCallback}>
          {orderedFrames.map((frame) => {
            const cells = FRAME_CELL_SIZES[frame.size] ?? 1;
            const cellSize = cells * CELL;
            return (
              <FrameCard
                key={frame.id}
                frame={frame}
                cellSize={cellSize}
                isSelected={selectedSet.has(frame.id)}
                wiringState={getWiringState(frame.id)}
                zoom={zoom}
                onPointerDown={onFramePointerDown}
                onContextMenu={onFrameContextMenu}
              />
            );
          })}
        </div>
      </div>
    );
  },
);
