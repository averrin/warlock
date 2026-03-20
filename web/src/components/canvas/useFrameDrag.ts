import { useRef, useCallback } from "react";
import { useGameStore } from "../../stores/game";
import type { FrameOverlayHandle } from "./FrameOverlay";
import {
  CELL,
  FRAME_CELL_SIZES,
  isGroupMoveValid,
  type ConnectionLike,
} from "./connectionGeometry";

interface UseFrameDragOptions {
  hostRef: React.RefObject<HTMLDivElement | null>;
  overlayRef: React.RefObject<FrameOverlayHandle | null>;
  framesRef: React.MutableRefObject<{ id: number; size: string; position?: { x: number; y: number } }[]>;
  framePositionsRef: React.MutableRefObject<Map<number, { x: number; y: number }>>;
  drawConnections: () => void;
  connectionsRef: React.MutableRefObject<readonly ConnectionLike[]>;
  moveFrame: (rpcClient: any, frameId: number, x: number, y: number) => Promise<void>;
  selectFrame: (id: number | null) => void;
  toggleFrameSelection: (id: number) => void;
  onFrameMiniInspect?: (
    frameId: number,
    clientX: number,
    clientY: number,
    opts?: { keepInPlace: boolean },
  ) => void;
}

export function useFrameDrag({
  hostRef,
  overlayRef,
  framesRef,
  framePositionsRef,
  drawConnections,
  connectionsRef,
  moveFrame,
  selectFrame,
  toggleFrameSelection,
  onFrameMiniInspect,
}: UseFrameDragOptions) {
  const draggingFrameIdsRef = useRef<Set<number> | null>(null);
  const dragMovedRef = useRef(false);
  const pendingMoveIds = useRef<Set<number>>(new Set());
  const rpcClientRef = useRef<any>(null);

  const dragOrigPosRef = useRef<Map<number, { x: number; y: number }>>(new Map());
  const dragStartCursorRef = useRef({ x: 0, y: 0 });

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

      const primaryBefore = useGameStore.getState().selectedFrameId;

      const mod = e.shiftKey || e.ctrlKey || e.metaKey;
      if (mod) {
        toggleFrameSelection(frameId);
        if (!useGameStore.getState().selectedFrameIds.includes(frameId)) {
          return;
        }
      } else {
        const beforeIds = useGameStore.getState().selectedFrameIds;
        if (!beforeIds.includes(frameId)) {
          selectFrame(frameId);
        }
      }

      const storeIds = useGameStore.getState().selectedFrameIds;
      if (storeIds.length <= 1) {
        const keepInPlace = primaryBefore === frameId;
        onFrameMiniInspect?.(frameId, e.clientX, e.clientY, { keepInPlace });
      }

      const groupIds = storeIds.filter((id) => framePositionsRef.current.has(id));
      if (groupIds.length === 0) return;

      const { wx, wy } = screenToWorld(e.clientX, e.clientY);
      draggingFrameIdsRef.current = new Set(groupIds);
      dragMovedRef.current = false;
      dragOrigPosRef.current = new Map();
      for (const id of groupIds) {
        const p = framePositionsRef.current.get(id);
        if (p) dragOrigPosRef.current.set(id, { x: p.x, y: p.y });
      }
      dragStartCursorRef.current = { x: wx, y: wy };

      const cardEl = (e.target as HTMLElement).closest("[data-frame-id]") as HTMLElement | null;
      if (!cardEl) {
        draggingFrameIdsRef.current = null;
        return;
      }

      cardEl.setPointerCapture(e.pointerId);

      const lastSnappedRef = new Map<number, { x: number; y: number }>();
      for (const id of groupIds) {
        const o = dragOrigPosRef.current.get(id);
        if (o) lastSnappedRef.set(id, { x: o.x, y: o.y });
      }

      const onMove = (me: PointerEvent) => {
        const group = draggingFrameIdsRef.current;
        if (!group || group.size === 0) return;

        const { wx: curWx, wy: curWy } = screenToWorld(me.clientX, me.clientY);
        const totalDx = curWx - dragStartCursorRef.current.x;
        const totalDy = curWy - dragStartCursorRef.current.y;

        const candidate = new Map<number, { x: number; y: number }>();
        for (const id of group) {
          const orig = dragOrigPosRef.current.get(id);
          if (!orig) continue;
          const x = Math.round((orig.x + totalDx) / CELL) * CELL;
          const y = Math.round((orig.y + totalDy) / CELL) * CELL;
          candidate.set(id, { x, y });
        }

        let changed = false;
        for (const id of group) {
          const next = candidate.get(id);
          const last = lastSnappedRef.get(id);
          if (!next || !last) continue;
          if (next.x !== last.x || next.y !== last.y) {
            changed = true;
            break;
          }
        }
        if (!changed) return;

        if (!isGroupMoveValid(candidate, connectionsRef.current, framesRef.current, framePositionsRef.current)) {
          return;
        }

        for (const [id, pos] of candidate) {
          lastSnappedRef.set(id, pos);
          framePositionsRef.current.set(id, pos);
        }

        dragMovedRef.current = true;

        const overlay = overlayRef.current;
        const host = hostRef.current;
        if (overlay && host) {
          const { scale, tx, ty } = overlay.getTransform();
          for (const id of group) {
            const pos = candidate.get(id);
            if (!pos) continue;
            const frame = framesRef.current.find((f) => f.id === id);
            const size = frame?.size ?? "S";
            const cells = FRAME_CELL_SIZES[size] ?? 1;
            const worldSize = cells * CELL;
            const el = host.querySelector(`[data-frame-id="${id}"]`) as HTMLElement | null;
            if (!el) continue;
            const screenX = pos.x * scale + tx;
            const screenY = pos.y * scale + ty;
            el.style.left = `${screenX / scale}px`;
            el.style.top = `${screenY / scale}px`;
            el.style.width = `${worldSize}px`;
            el.style.height = `${worldSize}px`;
            el.dataset.worldX = String(pos.x);
            el.dataset.worldY = String(pos.y);
          }
        }

        drawConnections();
      };

      const onUp = () => {
        const group = draggingFrameIdsRef.current;
        draggingFrameIdsRef.current = null;
        const moved = dragMovedRef.current;
        dragMovedRef.current = false;

        cardEl.removeEventListener("pointermove", onMove);
        cardEl.removeEventListener("pointerup", onUp);
        cardEl.removeEventListener("lostpointercapture", onUp);

        drawConnections();

        if (!moved || !group) return;

        for (const id of group) {
          const pos = framePositionsRef.current.get(id);
          if (!pos) continue;
          pendingMoveIds.current.add(id);
          moveFrame(rpcClientRef.current, id, pos.x, pos.y).finally(() => {
            pendingMoveIds.current.delete(id);
          });
        }
      };

      cardEl.addEventListener("pointermove", onMove);
      cardEl.addEventListener("pointerup", onUp);
      cardEl.addEventListener("lostpointercapture", onUp);
    },
    [
      selectFrame,
      toggleFrameSelection,
      onFrameMiniInspect,
      screenToWorld,
      framePositionsRef,
      framesRef,
      drawConnections,
      connectionsRef,
      moveFrame,
      overlayRef,
      hostRef,
    ],
  );

  return {
    handleFramePointerDown,
    draggingFrameIdsRef,
    pendingMoveIds,
    rpcClientRef,
  };
}
