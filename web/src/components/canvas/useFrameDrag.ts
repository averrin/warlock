import { useRef, useCallback } from "react";
import type { FrameOverlayHandle } from "./FrameOverlay";

const CELL = 75;

const FRAME_CELL_SIZES: Record<string, number> = {
  S: 1,
  M: 2,
  L: 3,
  G: 4,
};

interface UseFrameDragOptions {
  hostRef: React.RefObject<HTMLDivElement | null>;
  overlayRef: React.RefObject<FrameOverlayHandle | null>;
  framesRef: React.MutableRefObject<{ id: number; size: string; position?: { x: number; y: number } }[]>;
  framePositionsRef: React.MutableRefObject<Map<number, { x: number; y: number }>>;
  drawConnections: () => void;
  moveFrame: (rpcClient: any, frameId: number, x: number, y: number) => Promise<void>;
  selectFrame: (id: number | null) => void;
  onFrameMiniInspect?: (frameId: number, clientX: number, clientY: number) => void;
}

export function useFrameDrag({
  hostRef,
  overlayRef,
  framesRef,
  framePositionsRef,
  drawConnections,
  moveFrame,
  selectFrame,
  onFrameMiniInspect,
}: UseFrameDragOptions) {
  const draggingFrameIdRef = useRef<number | null>(null);
  const dragMovedRef = useRef(false);
  const pendingMoveIds = useRef<Set<number>>(new Set());
  const rpcClientRef = useRef<any>(null);

  // Stored once at drag start — the original world position and the starting cursor world coords
  const dragOrigPosRef = useRef({ x: 0, y: 0 });
  const dragStartCursorRef = useRef({ x: 0, y: 0 });

  /** Check if placing a frame at (x,y) with given size would overlap any other frame */
  const isOverlapping = useCallback(
    (x: number, y: number, size: string, excludeId: number | null) => {
      const cells = FRAME_CELL_SIZES[size] ?? 1;
      const w = cells * CELL;
      const h = w;
      for (const f of framesRef.current) {
        if (f.id === excludeId) continue;
        const pos = framePositionsRef.current.get(f.id);
        const fx = pos?.x ?? f.position?.x ?? 0;
        const fy = pos?.y ?? f.position?.y ?? 0;
        const fc = FRAME_CELL_SIZES[f.size] ?? 1;
        const fw = fc * CELL;
        const fh = fw;
        if (x < fx + fw && x + w > fx && y < fy + fh && y + h > fy) {
          return true;
        }
      }
      return false;
    },
    [framesRef, framePositionsRef],
  );

  /** Convert screen coordinates to world coordinates */
  const screenToWorld = useCallback(
    (clientX: number, clientY: number): { wx: number; wy: number } => {
      const host = hostRef.current;
      const overlay = overlayRef.current;
      if (!host || !overlay) return { wx: 0, wy: 0 };
      const rect = host.getBoundingClientRect();
      const sx = clientX - rect.left;
      const sy = clientY - rect.top;
      const { scale, tx, ty } = overlay.getTransform();
      return {
        wx: (sx - tx) / scale,
        wy: (sy - ty) / scale,
      };
    },
    [hostRef, overlayRef],
  );

  const handleFramePointerDown = useCallback(
    (e: React.PointerEvent, frameId: number) => {
      if (e.button !== 0) return;

      e.stopPropagation();
      selectFrame(frameId);
      onFrameMiniInspect?.(frameId, e.clientX, e.clientY);

      // Record the original frame position and the cursor start point
      const origPos = framePositionsRef.current.get(frameId);
      if (!origPos) return;

      const { wx, wy } = screenToWorld(e.clientX, e.clientY);
      draggingFrameIdRef.current = frameId;
      dragMovedRef.current = false;
      dragOrigPosRef.current = { x: origPos.x, y: origPos.y };
      dragStartCursorRef.current = { x: wx, y: wy };

      const cardEl = (e.target as HTMLElement).closest("[data-frame-id]") as HTMLElement | null;
      if (!cardEl) return;

      cardEl.setPointerCapture(e.pointerId);

      // Track the last snapped position to avoid redundant updates
      let lastSnappedX = origPos.x;
      let lastSnappedY = origPos.y;

      const onMove = (me: PointerEvent) => {
        const id = draggingFrameIdRef.current;
        if (id === null) return;

        const { wx: curWx, wy: curWy } = screenToWorld(me.clientX, me.clientY);

        // Total delta from drag start
        const totalDx = curWx - dragStartCursorRef.current.x;
        const totalDy = curWy - dragStartCursorRef.current.y;

        // New position = original + delta, snapped to grid
        const x = Math.round((dragOrigPosRef.current.x + totalDx) / CELL) * CELL;
        const y = Math.round((dragOrigPosRef.current.y + totalDy) / CELL) * CELL;

        // Skip if same grid position
        if (x === lastSnappedX && y === lastSnappedY) return;

        const frame = framesRef.current.find((f) => f.id === id);
        const size = frame?.size ?? "S";
        if (isOverlapping(x, y, size, id)) return;

        lastSnappedX = x;
        lastSnappedY = y;
        dragMovedRef.current = true;
        framePositionsRef.current.set(id, { x, y });

        // Update card position — must match FrameOverlay's zoom approach:
        // CSS zoom multiplies left/top, so divide screen coords by scale.
        const overlay = overlayRef.current;
        if (overlay) {
          const { scale, tx, ty } = overlay.getTransform();
          const cells = FRAME_CELL_SIZES[size] ?? 1;
          const worldSize = cells * CELL;
          const screenX = x * scale + tx;
          const screenY = y * scale + ty;
          cardEl.style.left = `${screenX / scale}px`;
          cardEl.style.top = `${screenY / scale}px`;
          cardEl.style.width = `${worldSize}px`;
          cardEl.style.height = `${worldSize}px`;
          // Also update data attrs so syncTransform stays correct
          cardEl.dataset.worldX = String(x);
          cardEl.dataset.worldY = String(y);
        }

        drawConnections();
      };

      const onUp = () => {
        const id = draggingFrameIdRef.current;
        draggingFrameIdRef.current = null;
        const moved = dragMovedRef.current;
        dragMovedRef.current = false;

        cardEl.removeEventListener("pointermove", onMove);
        cardEl.removeEventListener("pointerup", onUp);
        cardEl.removeEventListener("lostpointercapture", onUp);

        if (!moved || id === null) return;

        const pos = framePositionsRef.current.get(id);
        if (!pos) return;

        pendingMoveIds.current.add(id);
        moveFrame(rpcClientRef.current, id, pos.x, pos.y).finally(() => {
          pendingMoveIds.current.delete(id);
        });
      };

      cardEl.addEventListener("pointermove", onMove);
      cardEl.addEventListener("pointerup", onUp);
      cardEl.addEventListener("lostpointercapture", onUp);
    },
    [selectFrame, onFrameMiniInspect, screenToWorld, framePositionsRef, framesRef, isOverlapping, drawConnections, moveFrame, overlayRef],
  );

  return {
    handleFramePointerDown,
    draggingFrameIdRef,
    pendingMoveIds,
    rpcClientRef,
  };
}
