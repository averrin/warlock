import type { RpcClient } from "../../rpc/client";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { useGameStore } from "../../stores/game";
import { Panel } from "./Panel";

type Props = {
  rpcClient: RpcClient;
};

export function FrameInspector({ rpcClient }: Props) {
  const frames = useGameStore((s) => s.frames);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
  const selectFrame = useGameStore((s) => s.selectFrame);
  const activateFrame = useGameStore((s) => s.activateFrame);
  const supported = isFeatureSupported(capabilityMethods.frameInspector);

  const selected = frames.find((f) => f.id === selectedFrameId) ?? null;

  return (
    <Panel title="Frame Inspector" unsupported={supported ? undefined : "frame inspector"}>
      <div style={{ display: "flex", flexDirection: "column", gap: 8 }}>
        <select
          disabled={!supported}
          value={selectedFrameId ?? ""}
          onChange={(e) => selectFrame(Number(e.target.value) || null)}
        >
          <option value="">Select frame</option>
          {frames.map((frame) => (
            <option key={frame.id} value={frame.id}>
              {frame.name} ({frame.id})
            </option>
          ))}
        </select>
        {selected ? (
          <>
            <div style={{ fontSize: 12 }}>
              <div>name: {selected.name}</div>
              <div>size: {selected.size}</div>
              <div>components: {selected.component_count}</div>
            </div>
            <button
              disabled={!supported}
              onClick={() => {
                void activateFrame(rpcClient, selected.id);
              }}
            >
              Activate Frame
            </button>
          </>
        ) : (
          <div style={{ fontSize: 12, color: "#9ca3af" }}>No frame selected</div>
        )}
      </div>
    </Panel>
  );
}
