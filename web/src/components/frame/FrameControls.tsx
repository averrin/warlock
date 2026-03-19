import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { useConnectionStore } from "../../stores/connection";
import { IconButton } from "../ui";

type Props = {
  frameId: number;
  rpcClient: RpcClient;
  showRefresh?: boolean;
  showExpand?: boolean;
  onExpand?: () => void;
};

export function FrameControls({ frameId, rpcClient, showRefresh, showExpand, onExpand }: Props) {
  const activateFrame = useGameStore((s) => s.activateFrame);
  const deactivateFrame = useGameStore((s) => s.deactivateFrame);
  const fetchInitialState = useGameStore((s) => s.fetchInitialState);
  const claimed = useConnectionStore((s) => s.claimed);

  return (
    <div style={{ display: "flex", gap: 6 }}>
      <IconButton icon="▶" onClick={() => void activateFrame(rpcClient, frameId)} title="Activate frame" size="sm" disabled={!claimed} />
      <IconButton icon="⏹" onClick={() => void deactivateFrame(rpcClient, frameId)} title="Deactivate frame" size="sm" disabled={!claimed} />
      {showRefresh && (
        <IconButton icon="↻" onClick={() => void fetchInitialState(rpcClient)} title="Refresh" size="sm" disabled={!claimed} />
      )}
      {showExpand && onExpand && (
        <IconButton icon="⧉" onClick={onExpand} title="Expand" size="sm" />
      )}
    </div>
  );
}
