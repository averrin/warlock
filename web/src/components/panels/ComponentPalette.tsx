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

  // Component addition UI has been moved into the Frame Inspector Components tab
  // to keep frame-related actions in a single place. We keep this panel minimal
  // and deprecated to avoid duplicate controls.
  return (
    <Panel title="Component Palette" unsupported={supported ? undefined : "component add"}>
      <div style={{ fontSize: 12, color: "#9ca3af" }}>
        Component addition is now available in the Frame Inspector (Components tab).
      </div>
    </Panel>
  );
}
