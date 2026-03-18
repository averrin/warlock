import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { useWindowLayoutStore } from "../../stores/windowLayout";
import { Panel } from "./Panel";
import { FramePanel } from "../frame";

type Props = {
  rpcClient: RpcClient;
};

export function FrameMiniInspector({ rpcClient }: Props) {
  const frames = useGameStore((s) => s.frames);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
  const miniInspectorPinned = useWindowLayoutStore((s) => s.miniInspectorPinned);
  const miniInspectorFrameId = useWindowLayoutStore((s) => s.miniInspectorFrameId);
  const focusPanel = useWindowLayoutStore((s) => s.focusPanel);

  const effectiveFrameId = miniInspectorPinned && miniInspectorFrameId != null ? miniInspectorFrameId : selectedFrameId;
  const frame = frames.find((f) => f.id === effectiveFrameId) ?? null;

  if (!frame) {
    return (
      <Panel title="Frame Mini Inspector">
        <div style={{ fontSize: 12, color: "#9ca3af" }}>No frame selected</div>
      </Panel>
    );
  }

  return (
    <FramePanel
      frame={frame}
      rpcClient={rpcClient}
      defaultLevel={1}
      onNavigateToTree={() => focusPanel("state-editor")}
    />
  );
}
