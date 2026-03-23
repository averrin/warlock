import type { RpcClient } from "../../rpc/client";
import { SpendableBar } from "./SpendableBar";
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
        gridTemplateRows: "auto 1fr 36px",
        width: "100%",
        height: "100%",
        background: "#0a0a0a",
      }}
    >
      <SpendableBar rpcClient={rpcClient} />
      <WindowWorkspace rpcClient={rpcClient} />
      <StatusBar rpcClient={rpcClient} />
    </div>
  );
}
