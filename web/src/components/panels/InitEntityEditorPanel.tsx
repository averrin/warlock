import { useEffect, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { useSaveSlotsStore } from "../../stores/saveSlots";
import { EntitiesSection } from "./EcsEntityInspector";

type Props = {
  rpcClient: RpcClient;
};

export function InitEntityEditorPanel({ rpcClient }: Props) {
  const openInitName = useSaveSlotsStore((s) => s.openInitName);
  const saveInitEdits = useSaveSlotsStore((s) => s.saveInitEdits);
  const closeInit = useSaveSlotsStore((s) => s.closeInit);
  const [filter, setFilter] = useState("");
  const [saving, setSaving] = useState(false);

  useEffect(() => {
    if (!openInitName) setFilter("");
  }, [openInitName]);

  const handleSave = async () => {
    setSaving(true);
    try {
      await saveInitEdits(rpcClient);
    } finally {
      setSaving(false);
    }
  };

  const handleClose = async () => {
    await closeInit(rpcClient);
  };

  if (!openInitName) {
    return (
      <div style={{ padding: 16, color: "#6b7280", fontSize: 12 }}>
        No init state open. Use the <strong style={{ color: "#e5e7eb" }}>State Inspector → Init States</strong> panel to open one.
      </div>
    );
  }

  return (
    <div style={{ display: "flex", flexDirection: "column", height: "100%", color: "#e5e7eb", fontSize: 12 }}>
      {/* Header bar */}
      <div
        style={{
          display: "flex",
          alignItems: "center",
          gap: 8,
          padding: "6px 10px",
          borderBottom: "1px solid #1f2937",
          background: "#0b1220",
          flexShrink: 0,
        }}
      >
        <span style={{ fontWeight: 600, flex: 1 }}>Init: {openInitName}</span>
        <button
          type="button"
          disabled={saving}
          onClick={() => void handleSave()}
          style={{
            padding: "2px 10px",
            fontSize: 11,
            borderRadius: 3,
            border: "1px solid #374151",
            background: saving ? "#1f2937" : "#1d4ed8",
            color: "#e5e7eb",
            cursor: saving ? "not-allowed" : "pointer",
          }}
        >
          {saving ? "Saving…" : "Save"}
        </button>
        <button
          type="button"
          onClick={() => void handleClose()}
          style={{
            padding: "2px 10px",
            fontSize: 11,
            borderRadius: 3,
            border: "1px solid #374151",
            background: "#0b1220",
            color: "#9ca3af",
            cursor: "pointer",
          }}
        >
          Close
        </button>
      </div>

      {/* Filter input */}
      <div style={{ padding: "6px 8px", flexShrink: 0 }}>
        <input
          type="text"
          placeholder="Filter by name…"
          value={filter}
          onChange={(e) => setFilter(e.target.value)}
          style={{
            width: "100%",
            padding: "4px 8px",
            fontSize: 11,
            borderRadius: 4,
            border: "1px solid #374151",
            background: "#0b1220",
            color: "#e5e7eb",
            boxSizing: "border-box",
          }}
        />
      </div>

      {/* Entity list */}
      <div style={{ flex: 1, overflowY: "auto" }}>
        <EntitiesSection rpcClient={rpcClient} filter={filter} rpcPrefix="init.entities" />
      </div>
    </div>
  );
}
