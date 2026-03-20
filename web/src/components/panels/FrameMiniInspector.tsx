import { useEffect, type CSSProperties } from "react";
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

const rootStyle: CSSProperties = {
  height: "100%",
  minHeight: 0,
  display: "flex",
  flexDirection: "column",
  overflow: "hidden",
  boxSizing: "border-box",
};

const scrollStyle: CSSProperties = {
  flex: 1,
  minHeight: 0,
  overflowY: "auto",
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
      <div data-mini-inspector-panel style={rootStyle}>
        <div style={scrollStyle}>
          <Panel title="Frame Mini Inspector">
            <div style={{ fontSize: 12, color: "#9ca3af" }}>No frame selected</div>
          </Panel>
        </div>
      </div>
    );
  }

  return (
    <div data-mini-inspector-panel style={rootStyle}>
      <div style={scrollStyle}>
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
    </div>
  );
}
