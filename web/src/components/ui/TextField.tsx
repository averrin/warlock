import { useState } from "react";
import { inputStyle } from "./styles";

type Props = {
  label: string;
  value: string | undefined;
  onChange: (v: string) => void;
  labelWidth?: number;
};

export function TextField({ label, value, onChange, labelWidth = 90 }: Props) {
  const [editing, setEditing] = useState(false);
  const [draft, setDraft] = useState("");

  const startEdit = () => {
    setDraft(value ?? "");
    setEditing(true);
  };

  const commit = () => {
    onChange(draft);
    setEditing(false);
  };

  return (
    <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
      <span style={{ minWidth: labelWidth, color: "#9ca3af" }}>{label}:</span>
      {editing ? (
        <input
          type="text"
          value={draft}
          onChange={(e) => setDraft(e.target.value)}
          onBlur={commit}
          onKeyDown={(e) => {
            if (e.key === "Enter") commit();
            if (e.key === "Escape") setEditing(false);
          }}
          autoFocus
          style={{ ...inputStyle, width: 140 }}
        />
      ) : (
        <span onClick={startEdit} style={{ cursor: "pointer", color: "#e5e7eb" }}>
          {value || "-"}
        </span>
      )}
    </div>
  );
}
