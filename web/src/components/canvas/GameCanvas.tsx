import { useEffect, useMemo, useRef } from "react";
import { Application, Container, Graphics, Text } from "pixi.js";
import type { FederatedPointerEvent } from "pixi.js";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";

type Props = {
  rpcClient: RpcClient;
};

const GRID = 128;
type FrameNode = Container & {
  __box?: Graphics;
  __label?: Text;
  __idLabel?: Text;
};

export function GameCanvas({ rpcClient }: Props) {
  const hostRef = useRef<HTMLDivElement | null>(null);
  const appRef = useRef<Application | null>(null);
  const frameLayerRef = useRef<Container | null>(null);
  const frameNodeByIdRef = useRef<Map<number, Container>>(new Map());
  const frames = useGameStore((s) => s.frames);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
  const selectFrame = useGameStore((s) => s.selectFrame);
  const createFrameAt = useGameStore((s) => s.createFrameAt);
  const moveFrame = useGameStore((s) => s.moveFrame);
  const draggingFrameIdRef = useRef<number | null>(null);
  const dragMovedRef = useRef(false);
  const pendingDropRef = useRef<{ id: number; x: number; y: number } | null>(null);

  const placedFrames = useMemo(() => {
    return frames.map((frame, index) => {
      const x = frame.position?.x ?? (index % 6) * GRID + 40;
      const y = frame.position?.y ?? Math.floor(index / 6) * GRID + 40;
      return { ...frame, _x: x, _y: y };
    });
  }, [frames]);

  const onFrameMove = (event: FederatedPointerEvent) => {
    if (draggingFrameIdRef.current === null || !frameLayerRef.current) {
      return;
    }
    const id = draggingFrameIdRef.current;
    const local = event.getLocalPosition(frameLayerRef.current);
    const x = Math.round(local.x / GRID) * GRID;
    const y = Math.round(local.y / GRID) * GRID;
    dragMovedRef.current = true;
    pendingDropRef.current = { id, x, y };
    const node = frameNodeByIdRef.current.get(id);
    if (node) {
      node.x = x;
      node.y = y;
    }
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
    void moveFrame(rpcClient, drop.id, drop.x, drop.y);
  };

  const onBackgroundClick = async (event: FederatedPointerEvent) => {
    if (event.button !== 0) {
      return;
    }
    if (!frameLayerRef.current) {
      return;
    }
    const local = event.getLocalPosition(frameLayerRef.current);
    const x = Math.round(local.x / GRID) * GRID;
    const y = Math.round(local.y / GRID) * GRID;
    await createFrameAt(rpcClient, `Frame-${Date.now()}`, x, y);
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
    void app
      .init({
        width: host.clientWidth || 900,
        height: host.clientHeight || 700,
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

        const grid = new Graphics();
        grid.setStrokeStyle({ width: 1, color: 0x1f2937, alpha: 0.65 });
        for (let x = 0; x < app.screen.width; x += GRID) {
          grid.moveTo(x, 0);
          grid.lineTo(x, app.screen.height);
        }
        for (let y = 0; y < app.screen.height; y += GRID) {
          grid.moveTo(0, y);
          grid.lineTo(app.screen.width, y);
        }
        app.stage.addChild(grid);

        const frameLayer = new Container();
        frameLayerRef.current = frameLayer;
        frameLayer.eventMode = "static";
        frameLayer.on("pointermove", onFrameMove);
        frameLayer.on("pointerup", onFrameUp);
        frameLayer.on("pointerupoutside", onFrameUp);
        app.stage.addChild(frameLayer);
      });

    return () => {
      disposed = true;
      appRef.current = null;
      frameLayerRef.current = null;
      frameNodeByIdRef.current.clear();
      if (canvasEl && host.contains(canvasEl)) {
        host.removeChild(canvasEl);
      }
      if (initialized) {
        app.destroy();
      }
    };
  }, []);

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
          e.stopPropagation();
          draggingFrameIdRef.current = frameId;
          dragMovedRef.current = false;
          pendingDropRef.current = null;
          selectFrame(frameId);
        });

        const box = new Graphics();
        node.__box = box;
        node.addChild(box);

        const label = new Text({
          text: "",
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
          style: {
            fill: 0xcbd5e1,
            fontSize: 11,
          },
        });
        idLabel.x = 8;
        idLabel.y = 28;
        node.__idLabel = idLabel;
        node.addChild(idLabel);

        frameNodeByIdRef.current.set(frame.id, node);
        frameLayer.addChild(node);
      }

      node.x = frame._x;
      node.y = frame._y;
      if (node.__box) {
        node.__box.clear();
        node.__box.beginFill(selectedFrameId === frame.id ? 0x2563eb : 0x334155);
        node.__box.roundRect(0, 0, 110, 68, 10);
        node.__box.fill();
      }
      if (node.__label) {
        node.__label.text = frame.name;
      }
      if (node.__idLabel) {
        node.__idLabel.text = `id ${frame.id}`;
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
      if (e.target === app.stage) {
        void onBackgroundClick(e);
      }
    });
  }, [onBackgroundClick, placedFrames, selectFrame, selectedFrameId]);

  return (
    <div ref={hostRef} style={{ width: "100%", height: "100%" }} />
  );
}
