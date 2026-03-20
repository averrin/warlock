import { useEffect } from "react";
import type { DockviewPanelApi } from "dockview-core";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { useWindowLayoutStore } from "../../stores/windowLayout";
import { Panel } from "./Panel";
import { FramePanel } from "../frame";

type Props = {
  rpcClient: RpcClient;
  panelApi?: DockviewPanelApi;
};

export function FrameMiniInspector({ rpcClient, panelApi }: Props) {
  const frames = useGameStore((s) => s.frames);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
  const selectFrame = useGameStore((s) => s.selectFrame);
  const pinnedMiniInspectors = useWindowLayoutStore((s) => s.pinnedMiniInspectors);
  const focusStateInspectorExpandFrame = useWindowLayoutStore((s) => s.focusStateInspectorExpandFrame);

  const panelId = panelApi?.id ?? "frame-mini-inspector";
  const pinnedFrameId = pinnedMiniInspectors[panelId] ?? null;
  const isPinned = pinnedFrameId != null;

  const effectiveFrameId = isPinned ? pinnedFrameId : selectedFrameId;
  const frame = frames.find((f) => f.id === effectiveFrameId) ?? null;

  // Update panel title when frame changes
  useEffect(() => {
    if (!panelApi) return;
    if (frame) {
      const title = isPinned
        ? `📌 ${frame.name} #${frame.id}`
        : `${frame.name} #${frame.id}`;
      panelApi.setTitle(title);
    } else {
      panelApi.setTitle("Mini Inspector");
    }
  }, [panelApi, frame, isPinned]);

  if (!frame) {
    return (
      <div data-mini-inspector-panel>
        <Panel title="Frame Mini Inspector">
          <div style={{ fontSize: 12, color: "#9ca3af" }}>No frame selected</div>
        </Panel>
      </div>
    );
  }

  return (
    <div data-mini-inspector-panel>
      <FramePanel
        frame={frame}
        rpcClient={rpcClient}
        defaultLevel={1}
        onNavigateToTree={() => {
          selectFrame(frame.id);
          focusStateInspectorExpandFrame(frame.id);
        }}
      />
    </div>
  );
}
