import type { CSSProperties } from "react";

type Props = {
  expanded: boolean;
  onToggle: () => void;
};

/**
 * Two-slot expand/collapse control matching the FramePanel LevelControls schema.
 * Left slot is always an empty placeholder (keeps right slot position stable).
 * Right slot: ▸ when collapsed, ◂ when expanded — same pixel position so ◂ lands
 * under cursor after the user clicks ▸ to expand.
 */
export function CardExpandControl({ expanded, onToggle }: Props) {
  const btn: CSSProperties = {
    border: "1px solid #334155",
    borderRadius: 4,
    width: 20,
    height: 20,
    background: "#020617",
    color: "#9ca3af",
    cursor: "pointer",
    fontSize: 11,
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    padding: 0,
    flexShrink: 0,
  };

  return (
    <div style={{ display: "flex", gap: 2, alignItems: "center", flexShrink: 0 }}>
      {/* LEFT slot: always empty placeholder to keep right slot position stable */}
      <div style={{ width: 20, height: 20, flexShrink: 0 }} />
      {/* RIGHT slot: ▸ collapsed → ◂ expanded */}
      <button
        type="button"
        style={btn}
        title={expanded ? "Collapse" : "Expand"}
        onClick={(e) => { e.stopPropagation(); onToggle(); }}
      >
        {expanded ? "◂" : "▸"}
      </button>
    </div>
  );
}
