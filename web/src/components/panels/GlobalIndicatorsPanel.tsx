import { useState } from "react";
import { useGameStore } from "../../stores/game";
import { useRpcClient } from "../../hooks/useRpc";
import { Panel } from "./Panel";

export function GlobalIndicatorsPanel() {
  const rpcClient = useRpcClient();
  const indicators = useGameStore((s) => s.indicators);
  const markers = useGameStore((s) => s.markers);

  const [newLabel, setNewLabel] = useState("");
  const [newX, setNewX] = useState("0");
  const [newY, setNewY] = useState("0");
  const [newColor, setNewColor] = useState("#ff0000");

  const [editingLabel, setEditingLabel] = useState<string | null>(null);
  const [editLabel, setEditLabel] = useState("");
  const [editX, setEditX] = useState("");
  const [editY, setEditY] = useState("");
  const [editColor, setEditColor] = useState("#ff0000");

  const startEdit = (marker: (typeof markers)[number]) => {
    setEditingLabel(marker.label);
    setEditLabel(marker.label);
    setEditX(String(marker.x));
    setEditY(String(marker.y));
    setEditColor(/^#[0-9a-fA-F]{6}$/.test(marker.color) ? marker.color : "#ff0000");
  };

  const cancelEdit = () => {
    setEditingLabel(null);
  };

  const saveEdit = async () => {
    if (editingLabel == null) return;
    const trimmed = editLabel.trim();
    if (!trimmed) return;
    const x = parseFloat(editX);
    const y = parseFloat(editY);
    if (!Number.isFinite(x) || !Number.isFinite(y)) return;
    if (trimmed !== editingLabel) {
      await rpcClient.call("input.marker_remove", { label: editingLabel });
    }
    await rpcClient.call("input.marker_set", { x, y, label: trimmed, color: editColor });
    setEditingLabel(null);
  };

  const handleAddMarker = () => {
    if (!newLabel) return;
    const x = parseFloat(newX);
    const y = parseFloat(newY);
    void rpcClient.call("input.marker_set", { x, y, label: newLabel, color: newColor });
    setNewLabel("");
  };

  const handleRemoveMarker = (label: string) => {
    void rpcClient.call("input.marker_remove", { label });
  };

  return (
    <Panel title="Global Indicators & Markers">
      <div style={{ padding: 4, display: "flex", flexDirection: "column", gap: 12 }}>

        <div>
          <div style={{ color: "#94a3b8", fontSize: 11, marginBottom: 4, fontWeight: "bold" }}>Indicators</div>
          <div style={{ display: "flex", flexDirection: "column", gap: 4 }}>
            {Object.keys(indicators).length === 0 ? (
              <div style={{ color: "#6b7280", fontSize: 11 }}>No indicators</div>
            ) : (
              Object.entries(indicators).map(([key, indicator]) => (
                <div
                  key={key}
                  style={{
                    display: "flex",
                    justifyContent: "space-between",
                    padding: "4px 8px",
                    background: "#1e293b",
                    borderRadius: 4,
                    borderLeft: `4px solid ${indicator.color || "#3b82f6"}`,
                    fontSize: 12,
                  }}
                >
                  <span style={{ color: "#94a3b8" }}>{indicator.label}</span>
                  <span style={{ color: "#e5e7eb", fontWeight: 600 }}>{indicator.value}</span>
                </div>
              ))
            )}
          </div>
        </div>

        <div>
          <div style={{ color: "#94a3b8", fontSize: 11, marginBottom: 4, fontWeight: "bold" }}>Map Markers</div>

          <div style={{ display: "flex", flexDirection: "column", gap: 4, marginBottom: 8 }}>
            {markers.length === 0 ? (
              <div style={{ color: "#6b7280", fontSize: 11 }}>No markers</div>
            ) : (
              markers.map((marker) =>
                editingLabel === marker.label ? (
                  <div
                    key={marker.label}
                    style={{
                      display: "flex",
                      flexDirection: "column",
                      gap: 6,
                      padding: "6px 8px",
                      background: "#0f172a",
                      border: "1px solid #3b82f6",
                      borderRadius: 4,
                      fontSize: 11,
                    }}
                  >
                    <div style={{ display: "flex", gap: 4, flexWrap: "wrap", alignItems: "center" }}>
                      <input
                        type="text"
                        value={editLabel}
                        onChange={(e) => setEditLabel(e.target.value)}
                        style={{
                          flex: 1,
                          minWidth: 60,
                          fontSize: 11,
                          padding: "2px 4px",
                          background: "#020617",
                          border: "1px solid #334155",
                          color: "#e5e7eb",
                          borderRadius: 3,
                        }}
                      />
                      <input
                        type="number"
                        value={editX}
                        onChange={(e) => setEditX(e.target.value)}
                        style={{
                          width: 52,
                          fontSize: 11,
                          padding: "2px 4px",
                          background: "#020617",
                          border: "1px solid #334155",
                          color: "#e5e7eb",
                          borderRadius: 3,
                        }}
                      />
                      <input
                        type="number"
                        value={editY}
                        onChange={(e) => setEditY(e.target.value)}
                        style={{
                          width: 52,
                          fontSize: 11,
                          padding: "2px 4px",
                          background: "#020617",
                          border: "1px solid #334155",
                          color: "#e5e7eb",
                          borderRadius: 3,
                        }}
                      />
                      <input
                        type="color"
                        value={editColor}
                        onChange={(e) => setEditColor(e.target.value)}
                        style={{ width: 24, height: 20, padding: 0, border: "none", background: "transparent", cursor: "pointer" }}
                      />
                    </div>
                    <div style={{ display: "flex", gap: 6, justifyContent: "flex-end" }}>
                      <button
                        type="button"
                        onClick={cancelEdit}
                        style={{
                          fontSize: 11,
                          padding: "2px 8px",
                          background: "transparent",
                          border: "1px solid #475569",
                          borderRadius: 3,
                          color: "#94a3b8",
                          cursor: "pointer",
                        }}
                      >
                        Cancel
                      </button>
                      <button
                        type="button"
                        onClick={() => void saveEdit()}
                        style={{
                          fontSize: 11,
                          padding: "2px 8px",
                          background: "#1e3a5f",
                          border: "1px solid #334155",
                          borderRadius: 3,
                          color: "#e5e7eb",
                          cursor: "pointer",
                        }}
                      >
                        Save
                      </button>
                    </div>
                  </div>
                ) : (
                  <div
                    key={marker.label}
                    style={{
                      display: "flex",
                      alignItems: "center",
                      justifyContent: "space-between",
                      padding: "4px 8px",
                      background: "#0f172a",
                      border: "1px solid #334155",
                      borderRadius: 4,
                      fontSize: 11,
                    }}
                  >
                    <div style={{ display: "flex", alignItems: "center", gap: 6, minWidth: 0 }}>
                      <div style={{ width: 8, height: 8, borderRadius: "50%", flexShrink: 0, background: marker.color }} />
                      <span style={{ color: "#e5e7eb" }}>{marker.label}</span>
                      <span style={{ color: "#64748b" }}>
                        ({Math.round(marker.x)}, {Math.round(marker.y)})
                      </span>
                    </div>
                    <div style={{ display: "flex", alignItems: "center", gap: 4, flexShrink: 0 }}>
                      <button
                        type="button"
                        onClick={() => startEdit(marker)}
                        style={{
                          background: "transparent",
                          border: "none",
                          color: "#94a3b8",
                          cursor: "pointer",
                          fontSize: 11,
                          padding: "0 4px",
                        }}
                      >
                        Edit
                      </button>
                      <button
                        type="button"
                        onClick={() => handleRemoveMarker(marker.label)}
                        style={{
                          background: "transparent",
                          border: "none",
                          color: "#ef4444",
                          cursor: "pointer",
                          fontSize: 11,
                          padding: 0,
                        }}
                      >
                        ✕
                      </button>
                    </div>
                  </div>
                ),
              )
            )}
          </div>

          <div style={{ display: "flex", gap: 4, flexWrap: "wrap" }}>
            <input
              type="text"
              placeholder="Label"
              value={newLabel}
              onChange={(e) => setNewLabel(e.target.value)}
              style={{ flex: 1, minWidth: 60, fontSize: 11, padding: "2px 4px", background: "#020617", border: "1px solid #334155", color: "#e5e7eb", borderRadius: 3 }}
            />
            <input
              type="number"
              placeholder="X"
              value={newX}
              onChange={(e) => setNewX(e.target.value)}
              style={{ width: 40, fontSize: 11, padding: "2px 4px", background: "#020617", border: "1px solid #334155", color: "#e5e7eb", borderRadius: 3 }}
            />
            <input
              type="number"
              placeholder="Y"
              value={newY}
              onChange={(e) => setNewY(e.target.value)}
              style={{ width: 40, fontSize: 11, padding: "2px 4px", background: "#020617", border: "1px solid #334155", color: "#e5e7eb", borderRadius: 3 }}
            />
            <input
              type="color"
              value={newColor}
              onChange={(e) => setNewColor(e.target.value)}
              style={{ width: 24, height: 20, padding: 0, border: "none", background: "transparent", cursor: "pointer" }}
            />
            <button
              type="button"
              onClick={handleAddMarker}
              style={{ fontSize: 11, padding: "2px 6px", background: "#1e3a5f", border: "1px solid #334155", borderRadius: 3, color: "#e5e7eb", cursor: "pointer" }}
            >
              Add
            </button>
          </div>
        </div>

      </div>
    </Panel>
  );
}
