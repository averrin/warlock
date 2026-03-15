import type { RpcClient } from "../../rpc/client";
import { GameCanvas } from "../canvas/GameCanvas";
import { Sidebar } from "./Sidebar";
import { StatusBar } from "./StatusBar";

type Props = {
  rpcClient: RpcClient;
};

export function AppShell({ rpcClient }: Props) {
  return (
    <div
      style={{
        display: "grid",
        gridTemplateColumns: "320px 1fr",
        gridTemplateRows: "1fr 36px",
        width: "100%",
        height: "100%",
        background: "#0a0a0a",
      }}
    >
      <Sidebar rpcClient={rpcClient} />
      <GameCanvas rpcClient={rpcClient} />
      <StatusBar />
    </div>
  );
}
