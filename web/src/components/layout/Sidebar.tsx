import type { RpcClient } from "../../rpc/client";
import { FrameInspector } from "../panels/FrameInspector";
import { PowerPanel } from "../panels/PowerPanel";
import { CodeEditorPanel } from "../panels/CodeEditorPanel";
import { EnvironmentPanel } from "../panels/EnvironmentPanel";
import { BlueprintPalette } from "../panels/BlueprintPalette";
import { SurfacePalette } from "../panels/SurfacePalette";
import { ComponentPalette } from "../panels/ComponentPalette";

type Props = {
  rpcClient: RpcClient;
};

export function Sidebar({ rpcClient }: Props) {
  return (
    <div
      style={{
        overflow: "auto",
        borderRight: "1px solid #1f2937",
        background: "#0f172a",
        padding: 12,
        display: "flex",
        flexDirection: "column",
        gap: 12,
      }}
    >
      <FrameInspector rpcClient={rpcClient} />
      <PowerPanel rpcClient={rpcClient} />
      <EnvironmentPanel />
      <BlueprintPalette rpcClient={rpcClient} />
      <SurfacePalette rpcClient={rpcClient} />
      <ComponentPalette rpcClient={rpcClient} />
      <CodeEditorPanel rpcClient={rpcClient} />
    </div>
  );
}
