import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { useConnectionStore } from "../../stores/connection";
import { IconButton } from "../ui";

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
  const claimed = useConnectionStore((s) => s.claimed);

  const showRepair = componentState === "COMP_ERROR" || componentState === "BROKEN";

  return (
    <div style={{ display: "flex", gap: 4, alignItems: "center" }}>
      <IconButton
        icon="▶"
        size="sm"
        title="Activate"
        onClick={() => void setComponentState(rpcClient, frameId, componentId, "active")}
        disabled={!claimed}
      />
      <IconButton
        icon="⏹"
        size="sm"
        title="Deactivate"
        onClick={() => void setComponentState(rpcClient, frameId, componentId, "inactive")}
        disabled={!claimed}
      />
      {showRepair && (
        <IconButton
          icon="⚙"
          size="sm"
          title="Repair"
          onClick={() => void repairComponent(rpcClient, frameId, componentId)}
          disabled={!claimed}
        />
      )}
      <IconButton
        icon="✕"
        size="sm"
        variant="danger"
        title="Delete"
        onClick={() => void removeComponent(rpcClient, frameId, componentId)}
        disabled={!claimed}
      />
    </div>
  );
}
