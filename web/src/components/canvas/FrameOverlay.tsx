import { forwardRef, useImperativeHandle, useRef, useCallback, useLayoutEffect } from "react";
import { FrameCard, type PlacedFrame, type WiringState } from "./FrameCard";
import type { ConnectionDTO } from "../../rpc/types";

const CELL = 75;

const FRAME_CELL_SIZES: Record<string, number> = {
  S: 1,
  M: 2,
  L: 3,
  G: 4,
};

export interface FrameOverlayHandle {
  /** Update all card positions to match Pixi worldContainer. Direct DOM writes — no React re-render. */
  syncTransform(scale: number, tx: number, ty: number): void;
  /** Get the current transform values */
  getTransform(): { scale: number; tx: number; ty: number };
}

interface FrameOverlayProps {
  placedFrames: PlacedFrame[];
  selectedFrameId: number | null;
  wiringMode: { sourceId: number; type: string } | null;
  connections: ConnectionDTO[];
  frames: PlacedFrame[];
  zoom: number;
  onFramePointerDown: (e: React.PointerEvent, frameId: number) => void;
  onFrameContextMenu: (e: React.MouseEvent, frameId: number) => void;
}

function frameHasConnector(
  frame: { components?: { name: string }[] },
  type: string,
): boolean {
  const CONNECTOR_COMPONENT: Record<string, string> = {
    POWER: "Power Wire Connector",
    DATA: "Data Wire Connector",
    CONVEYOR: "Conveyor Connector",
  };
  const compName = CONNECTOR_COMPONENT[type];
  if (!compName) return false;
  return (frame.components ?? []).some((c) => c.name === compName);
}

function frameMeetsConnectionRequirements(
  frame: { components?: { name: string }[] },
  type: string,
): boolean {
  if (type === "CONVEYOR") {
    return (frame.components ?? []).some((c) => c.name === "Storage");
  }
  if (type === "DATA") {
    return (frame.components ?? []).some((c) => c.name === "Core");
  }
  return true;
}

function isCardinallySatisfied(
  conns: ConnectionDTO[],
  frames: { id: number; components?: any[] }[],
  frameId: number,
  type: string,
): boolean {
  const CONNECTOR_COMPONENT: Record<string, string> = {
    POWER: "Power Wire Connector",
    DATA: "Data Wire Connector",
    CONVEYOR: "Conveyor Connector",
  };
  const frame = frames.find((f) => f.id === frameId);
  if (!frame) return false;
  const connectorName = CONNECTOR_COMPONENT[type];
  const connector = (frame.components ?? []).find((c: any) => c.name === connectorName);
  const attr =
    (connector as any)?.metadata?.attributes?.max_connections ??
    (connector?.attributes as Record<string, unknown> | undefined)?.max_connections;
  let max = type === "POWER" ? 10 : 1;
  if (attr && typeof (attr as any).final_value !== "undefined") {
    const v = (attr as any).final_value;
    const parsed = typeof v === "number" ? v : Number(v);
    if (!Number.isNaN(parsed)) max = parsed;
  }
  const count = conns.filter(
    (c) => (c.source === frameId || c.target === frameId) && c.type === type,
  ).length;
  return count < max;
}

function hasConnectionBetween(
  conns: ConnectionDTO[],
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

export const FrameOverlay = forwardRef<FrameOverlayHandle, FrameOverlayProps>(
  function FrameOverlay(
    {
      placedFrames,
      selectedFrameId,
      wiringMode,
      connections,
      frames,
      zoom,
      onFramePointerDown,
      onFrameContextMenu,
    },
    ref,
  ) {
    const containerRef = useRef<HTMLDivElement>(null);
    const transformRef = useRef({ scale: 1, tx: 0, ty: 0 });

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

        const isEligible =
          frameHasConnector(
            frames.find((f) => f.id === frameId) ?? { components: [] },
            wiringMode.type,
          ) &&
          frameMeetsConnectionRequirements(
            frames.find((f) => f.id === frameId) ?? { components: [] },
            wiringMode.type,
          ) &&
          isCardinallySatisfied(connections, frames, frameId, wiringMode.type) &&
          !hasConnectionBetween(connections, wiringMode.sourceId, frameId, wiringMode.type);

        return { type: wiringMode.type, isSource: false, isEligible };
      },
      [wiringMode, connections, frames],
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
          {placedFrames.map((frame) => {
            const cells = FRAME_CELL_SIZES[frame.size] ?? 1;
            const cellSize = cells * CELL;
            return (
              <FrameCard
                key={frame.entity_id}
                frame={frame}
                cellSize={cellSize}
                isSelected={selectedFrameId === frame.id}
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
