import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { smallBtnStyle } from "../ui";

type Props = {
  frameId: number;
  componentId: number;
  componentState: string;
  rpcClient: RpcClient;
};

export function ComponentControls({ frameId, componentId, componentState, rpcClient }: Props) {
  const setComponentState = useGameStore((s) => s.setComponentState);
  const repairComponent = useGameStore((s) => s.repairComponent);
  const removeComponent = useGameStore((s) => s.removeComponent);

  const showRepair = componentState === "COMP_ERROR" || componentState === "BROKEN";

  return (
    <div style={{ display: "flex", gap: 4, flexWrap: "wrap" }}>
      <button type="button" style={smallBtnStyle} onClick={() => void setComponentState(rpcClient, frameId, componentId, "active")}>
        Activate
      </button>
      <button type="button" style={smallBtnStyle} onClick={() => void setComponentState(rpcClient, frameId, componentId, "inactive")}>
        Deactivate
      </button>
      {showRepair && (
        <button type="button" style={smallBtnStyle} onClick={() => void repairComponent(rpcClient, frameId, componentId)}>
          Repair
        </button>
      )}
      <button
        type="button"
        style={{ ...smallBtnStyle, color: "#ef4444" }}
        onClick={() => void removeComponent(rpcClient, frameId, componentId)}
      >
        Delete
      </button>
    </div>
  );
}
