import { useState } from "react";
import { Panel } from "./Panel";

const toCssHex = (color: number) => `#${color.toString(16).padStart(6, "0")}`;

const COLORS = [
  { r: 239, g: 68, b: 68, a: 128 },
  { r: 249, g: 115, b: 22, a: 128 },
  { r: 234, g: 179, b: 8, a: 128 },
  { r: 34, g: 197, b: 94, a: 128 },
  { r: 6, g: 182, b: 212, a: 128 },
  { r: 59, g: 130, b: 246, a: 128 },
  { r: 139, g: 92, b: 246, a: 128 },
  { r: 236, g: 72, b: 153, a: 128 },
  { r: 100, g: 116, b: 139, a: 128 },
];

export function SurfacePalette() {
  const [selectedColor, setSelectedColor] = useState(COLORS[3]!);
  const [material, setMaterial] = useState("concrete");

  return (
    <Panel title="Surface Palette">
      <div style={{ display: "flex", flexDirection: "column", gap: 12 }}>
        <div>
          <label style={{ display: "block", marginBottom: 4, fontSize: 12, color: "#9ca3af" }}>Material</label>
          <select
            value={material}
            onChange={(e) => setMaterial(e.target.value)}
            style={{ width: "100%", padding: 4, background: "#1e293b", border: "1px solid #334155", color: "white" }}
          >
            <option value="concrete">Concrete</option>
            <option value="metal">Metal</option>
            <option value="danger">Danger Zone</option>
            <option value="safe">Safe Zone</option>
          </select>
        </div>

        <div>
          <label style={{ display: "block", marginBottom: 4, fontSize: 12, color: "#9ca3af" }}>Color</label>
          <div style={{ display: "flex", gap: 4, flexWrap: "wrap" }}>
            {COLORS.map((c, i) => {
              const hex = toCssHex((c.r << 16) | (c.g << 8) | c.b);
              const isSelected = selectedColor.r === c.r && selectedColor.g === c.g && selectedColor.b === c.b;
              return (
                <button
                  key={i}
                  title={hex}
                  style={{
                    width: 24,
                    height: 24,
                    borderRadius: 4,
                    background: hex,
                    border: isSelected ? "2px solid white" : "1px solid #334155",
                    cursor: "pointer",
                    padding: 0
                  }}
                  onClick={() => setSelectedColor(c)}
                />
              );
            })}
          </div>
        </div>

        <button
          onClick={() => {
            const ev = new CustomEvent("warlock:createSurface", {
              detail: { material, color: selectedColor, icon: material }
            });
            window.dispatchEvent(ev);
          }}
          style={{ width: "100%", padding: "8px 0", marginTop: 8 }}
        >
          Draw Surface
        </button>
      </div>
    </Panel>
  );
}
