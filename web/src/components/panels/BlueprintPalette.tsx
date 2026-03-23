import { useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { useCanvasPlacementStore } from "../../stores/canvasPlacement";
import { Panel } from "./Panel";
import { BlueprintPicker } from "../picker";

type Props = {
  rpcClient: RpcClient;
};

export function BlueprintPalette({ rpcClient }: Props) {
  const [pickerOpen, setPickerOpen] = useState(false);
  const supported = isFeatureSupported(capabilityMethods.blueprintPalette);

  return (
    <Panel title="Blueprint Palette" unsupported={supported ? undefined : "blueprint workflow"}>
      <button
        disabled={!supported}
        onClick={() => setPickerOpen(true)}
        style={{ width: "100%" }}
      >
        Create from Blueprint...
      </button>
      <BlueprintPicker
        isOpen={pickerOpen}
        onClose={() => setPickerOpen(false)}
        rpcClient={rpcClient}
        onSelect={(data) => {
          useCanvasPlacementStore.getState().setMode({
            kind: "frame",
            blueprint: data.name,
            frameSizeKey: data.size,
          });
          setPickerOpen(false);
        }}
      />
    </Panel>
  );
}
