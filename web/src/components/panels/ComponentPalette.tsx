import { useEffect, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { useGameStore } from "../../stores/game";
import { Panel } from "./Panel";

type Props = {
  rpcClient: RpcClient;
};

export function ComponentPalette({ rpcClient }: Props) {
  const [components, setComponents] = useState<string[]>([]);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
  const addComponent = useGameStore((s) => s.addComponent);
  const supported = isFeatureSupported(capabilityMethods.componentPalette);

  useEffect(() => {
    if (!supported) {
      return;
    }
    void rpcClient
      .call<{ sources: Record<string, string> }>("code.sources")
      .then((data) => setComponents(Object.keys(data.sources ?? {})))
      .catch(() => setComponents([]));
  }, [rpcClient, supported]);

  return (
    <Panel title="Component Palette" unsupported={supported ? undefined : "component add"}>
      <div style={{ display: "grid", gap: 8 }}>
        <select id="component-select">
          <option value="">Select component source</option>
          {components.map((name) => (
            <option key={name} value={name}>
              {name}
            </option>
          ))}
        </select>
        <button
          disabled={!supported || selectedFrameId === null}
          onClick={() => {
            if (selectedFrameId === null) {
              return;
            }
            const select = document.getElementById("component-select") as HTMLSelectElement | null;
            if (!select?.value) {
              return;
            }
            void addComponent(rpcClient, selectedFrameId, select.value);
          }}
        >
          Add to Selected Frame
        </button>
      </div>
    </Panel>
  );
}
