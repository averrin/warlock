import { useState } from "react";
import { inputStyle } from "./styles";

type Props = {
  label: string;
  value: number | undefined;
  onChange: (v: number) => void;
  labelWidth?: number;
  /** When set, non-edit display uses fixed decimal places. */
  precision?: number;
};

function formatDisplay(n: number, precision?: number): string {
  if (precision === undefined) return String(n);
  return n.toFixed(precision);
}

export function NumberField({ label, value, onChange, labelWidth = 90, precision }: Props) {
  const [editing, setEditing] = useState(false);
  const [draft, setDraft] = useState("");

  const startEdit = () => {
    const v = value ?? 0;
    setDraft(precision !== undefined ? v.toFixed(precision) : String(v));
    setEditing(true);
  };

  const commit = () => {
    const n = parseFloat(draft);
    if (!Number.isNaN(n)) onChange(n);
    setEditing(false);
  };

  return (
    <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
      <span style={{ minWidth: labelWidth, color: "#9ca3af" }}>{label}:</span>
      {editing ? (
        <input
          type="number"
          value={draft}
          onChange={(e) => setDraft(e.target.value)}
          onBlur={commit}
          onKeyDown={(e) => {
            if (e.key === "Enter") commit();
            if (e.key === "Escape") setEditing(false);
          }}
          autoFocus
          style={inputStyle}
        />
      ) : (
        <span onClick={startEdit} style={{ cursor: "pointer", color: "#e5e7eb" }}>
          {value != null ? formatDisplay(value, precision) : "-"}
        </span>
      )}
    </div>
  );
}
