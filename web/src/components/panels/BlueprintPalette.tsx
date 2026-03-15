import { useEffect, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { useGameStore } from "../../stores/game";
import { Panel } from "./Panel";

type Props = {
  rpcClient: RpcClient;
};

export function BlueprintPalette({ rpcClient }: Props) {
  const [blueprints, setBlueprints] = useState<string[]>([]);
  const createFromBlueprint = useGameStore((s) => s.createFromBlueprint);
  const supported = isFeatureSupported(capabilityMethods.blueprintPalette);

  useEffect(() => {
    if (!supported) {
      return;
    }
    void rpcClient
      .call<{ blueprints: Record<string, string> }>("code.blueprints")
      .then((data) => setBlueprints(Object.keys(data.blueprints ?? {})))
      .catch(() => setBlueprints([]));
  }, [rpcClient, supported]);

  return (
    <Panel title="Blueprint Palette" unsupported={supported ? undefined : "blueprint workflow"}>
      <div style={{ display: "flex", gap: 6 }}>
        <select id="blueprint-select" style={{ flex: 1 }}>
          <option value="">Select blueprint</option>
          {blueprints.map((name) => (
            <option key={name} value={name}>
              {name}
            </option>
          ))}
        </select>
        <button
          disabled={!supported}
          onClick={() => {
            const select = document.getElementById("blueprint-select") as HTMLSelectElement | null;
            if (!select?.value) {
              return;
            }
            void createFromBlueprint(rpcClient, select.value);
          }}
        >
          Create
        </button>
      </div>
    </Panel>
  );
}
