import type { CSSProperties } from "react";

export const inputStyle: CSSProperties = {
  width: 80,
  padding: "1px 4px",
  fontSize: 11,
  borderRadius: 3,
  border: "1px solid #374151",
  background: "#0b1220",
  color: "#e5e7eb",
};

export const smallBtnStyle: CSSProperties = {
  padding: "1px 6px",
  fontSize: 10,
  borderRadius: 3,
  border: "1px solid #374151",
  background: "#111827",
  color: "#e5e7eb",
  cursor: "pointer",
};

export const btnBase: CSSProperties = {
  padding: "2px 8px",
  fontSize: 11,
  border: "1px solid #374151",
  borderRadius: 4,
  cursor: "pointer",
  background: "#1f2937",
  color: "#e5e7eb",
};

export const btnActive: CSSProperties = {
  ...btnBase,
  background: "#3b82f6",
  borderColor: "#3b82f6",
  color: "#fff",
};
