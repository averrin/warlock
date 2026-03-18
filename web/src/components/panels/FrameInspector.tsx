import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { Panel } from "./Panel";
import { FramePanel } from "../frame";

type Props = {
  rpcClient: RpcClient;
};

export function FrameInspector({ rpcClient }: Props) {
  const frames = useGameStore((s) => s.frames);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);

  const frame = frames.find((f) => f.id === selectedFrameId) ?? null;

  if (!frame) {
    return (
      <Panel title="Frame Inspector">
        <div style={{ fontSize: 12, color: "#9ca3af" }}>No frame selected</div>
      </Panel>
    );
  }

  return (
    <FramePanel
      frame={frame}
      rpcClient={rpcClient}
      defaultLevel={2}
      showExpandButton={false}
    />
  );
}
