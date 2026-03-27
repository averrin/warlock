import { useState } from "react";
import { NumberField } from "./NumberField";
import { inputStyle, smallBtnStyle } from "./styles";

export interface DictEditorProps {
  data: Record<string, number>;
  onChangeEntry: (key: string, value: number) => void;
  onDeleteEntry?: (key: string) => void;
  onAddEntry?: (key: string, value: number) => void;
  readOnly?: boolean;
}

export function DictEditor({
  data,
  onChangeEntry,
  onDeleteEntry,
  onAddEntry,
  readOnly,
}: DictEditorProps) {
  const [newKey, setNewKey] = useState("");
  const [newVal, setNewVal] = useState("0");

  const rows = Object.entries(data);

  return (
    <div style={{ display: "grid", gap: 4 }}>
      {rows.map(([k, v]) => (
        <div key={k} style={{ display: "flex", alignItems: "center", gap: 6, minWidth: 0 }}>
          <div style={{ flex: 1, minWidth: 0 }}>
            {readOnly ? (
              <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
                <span style={{ minWidth: 160, color: "#9ca3af", fontSize: 11 }}>{k}:</span>
                <span style={{ color: "#e5e7eb", fontSize: 11 }}>{v}</span>
              </div>
            ) : (
              <NumberField label={k} value={v} onChange={(n) => onChangeEntry(k, n)} labelWidth={160} />
            )}
          </div>
          {onDeleteEntry && !readOnly && (
            <button
              type="button"
              title="Remove"
              style={{ ...smallBtnStyle, color: "#ef4444", flexShrink: 0, padding: "0 6px" }}
              onClick={() => onDeleteEntry(k)}
            >
              ×
            </button>
          )}
        </div>
      ))}
      {onAddEntry && !readOnly && (
        <div style={{ display: "flex", flexWrap: "wrap", gap: 6, alignItems: "center", marginTop: 2 }}>
          <input
            type="text"
            placeholder="key"
            value={newKey}
            onChange={(e) => setNewKey(e.target.value)}
            style={{ ...inputStyle, flex: "1 1 120px", minWidth: 100, fontSize: 11 }}
          />
          <input
            type="number"
            value={newVal}
            onChange={(e) => setNewVal(e.target.value)}
            style={{ ...inputStyle, width: 88, fontSize: 11 }}
          />
          <button
            type="button"
            style={smallBtnStyle}
            onClick={() => {
              const key = newKey.trim();
              if (!key) return;
              const n = parseFloat(newVal);
              if (Number.isNaN(n)) return;
              onAddEntry(key, n);
              setNewKey("");
              setNewVal("0");
            }}
          >
            Add
          </button>
        </div>
      )}
    </div>
  );
}
