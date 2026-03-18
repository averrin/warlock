import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
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

  return (
    <div style={{ display: "flex", gap: 6 }}>
      <IconButton icon="▶" onClick={() => void activateFrame(rpcClient, frameId)} title="Activate frame" size="sm" />
      <IconButton icon="⏹" onClick={() => void deactivateFrame(rpcClient, frameId)} title="Deactivate frame" size="sm" />
      {showRefresh && (
        <IconButton icon="↻" onClick={() => void fetchInitialState(rpcClient)} title="Refresh" size="sm" />
      )}
      {showExpand && onExpand && (
        <IconButton icon="⧉" onClick={onExpand} title="Expand" size="sm" />
      )}
    </div>
  );
}
