import type { RpcClient } from "../../rpc/client";
import { WindowWorkspace } from "./WindowWorkspace";
import { StatusBar } from "./StatusBar";

type Props = {
  rpcClient: RpcClient;
};

export function AppShell({ rpcClient }: Props) {
  return (
    <div
      style={{
        display: "grid",
        gridTemplateRows: "1fr 36px",
        width: "100%",
        height: "100%",
        background: "#0a0a0a",
      }}
    >
      <WindowWorkspace rpcClient={rpcClient} />
      <StatusBar rpcClient={rpcClient} />
    </div>
  );
}
