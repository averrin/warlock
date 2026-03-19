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
              markers.map((marker) => (
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
                  <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
                    <div style={{ width: 8, height: 8, borderRadius: "50%", background: marker.color }} />
                    <span style={{ color: "#e5e7eb" }}>{marker.label}</span>
                    <span style={{ color: "#64748b" }}>
                      ({Math.round(marker.x)}, {Math.round(marker.y)})
                    </span>
                  </div>
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
              ))
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
