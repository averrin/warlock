import type { CSSProperties } from "react";
import type { RpcClient } from "../../rpc/client";
import { useConnectionStore } from "../../stores/connection";
import { useGameStore } from "../../stores/game";

type Props = {
  frameId: number;
  componentId: number;
  rpcClient: RpcClient;
};

const DIRS: { dir: string; label: string; title: string }[] = [
  { dir: "up", label: "↑", title: "Move up" },
  { dir: "down", label: "↓", title: "Move down" },
  { dir: "left", label: "←", title: "Move left" },
  { dir: "right", label: "→", title: "Move right" },
];

const btn: CSSProperties = {
  width: 32,
  height: 28,
  padding: 0,
  fontSize: 14,
  border: "1px solid #334155",
  borderRadius: 4,
  background: "#0f172a",
  color: "#e2e8f0",
  cursor: "pointer",
};

export function PropulsionMoveControls({ frameId, componentId, rpcClient }: Props) {
  const claimed = useConnectionStore((s) => s.claimed);
  const callComponentApi = useGameStore((s) => s.callComponentApi);

  return (
    <div style={{ marginTop: 4 }}>
      <div style={{ fontSize: 10, color: "#94a3b8", textTransform: "uppercase", letterSpacing: "0.04em", marginBottom: 6 }}>
        Propulsion
      </div>
      <div style={{ display: "grid", gridTemplateColumns: "repeat(4, 32px)", gap: 4 }}>
        {DIRS.map(({ dir, label, title }) => (
          <button
            key={dir}
            type="button"
            title={title}
            disabled={!claimed}
            style={{ ...btn, opacity: claimed ? 1 : 0.45 }}
            onClick={(e) => {
              e.stopPropagation();
              void callComponentApi(rpcClient, frameId, componentId, "move", [dir]);
            }}
          >
            {label}
          </button>
        ))}
      </div>
    </div>
  );
}
