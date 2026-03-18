import { useState } from "react";
import type { RpcClient } from "../../rpc/client";
import type { ComponentDTO } from "../../rpc/types";
import { useGameStore } from "../../stores/game";
import { StoragePanel } from "../storage/StoragePanel";

type Props = {
  frameId: number;
  components: ComponentDTO[];
  rpcClient: RpcClient;
};

export function StorageBar({ frameId, components, rpcClient }: Props) {
  const [expanded, setExpanded] = useState(false);
  const fetchInitialState = useGameStore((s) => s.fetchInitialState);

  const compWithStorage = components.find((c) => c.storage) ?? null;
  if (!compWithStorage?.storage) return null;

  const storage = compWithStorage.storage;
  const used = storage.slots_used ?? storage.slots.filter((s) => s.stack).length;
  const total = storage.slots_total ?? storage.slots_count ?? storage.slots.length;
  const topItems = storage.top_items ?? [];

  if (!expanded) {
    return (
      <div style={{ display: "flex", alignItems: "center", gap: 6, borderTop: "1px solid #1e293b", paddingTop: 6 }}>
        <span style={{ fontSize: 11, color: "#9ca3af" }}>
          📦 {used}/{total}
          {topItems.length > 0 && <span style={{ marginLeft: 6 }}>{topItems.map((t) => t.name).join(", ")}</span>}
        </span>
        <button type="button" onClick={() => setExpanded(true)} style={{ fontSize: 10, padding: "2px 6px" }}>
          Expand
        </button>
      </div>
    );
  }

  return (
    <div style={{ borderTop: "1px solid #1e293b", paddingTop: 6 }}>
      <div style={{ display: "flex", alignItems: "center", gap: 6, marginBottom: 4 }}>
        <span style={{ fontSize: 11, fontWeight: 600, color: "#9ca3af" }}>Storage</span>
        <button type="button" onClick={() => setExpanded(false)} style={{ fontSize: 10, padding: "2px 6px" }}>
          Collapse
        </button>
      </div>
      <StoragePanel
        storage={storage}
        frameId={frameId}
        componentId={compWithStorage.id}
        mode="full"
        rpcClient={rpcClient}
        onRefresh={async () => fetchInitialState(rpcClient)}
      />
    </div>
  );
}
