import { useState } from "react";
import type { FrameDTO, PowerNetworkDTO } from "../../rpc/types";
import { Badge } from "../ui";
import { netAvailable } from "../../utils/power";

type Props = {
  frame: FrameDTO;
  powerNetwork?: PowerNetworkDTO | null;
  compact?: boolean;
  onRename?: (name: string) => void;
};

export function FrameHeader({ frame, powerNetwork, compact, onRename }: Props) {
  const [renaming, setRenaming] = useState<string | null>(null);

  const healthBadge = frame.canvas_badges?.has_error
    ? "ERROR"
    : (frame.canvas_badges?.health ?? "OK").toUpperCase();

  const netAvail = powerNetwork ? netAvailable(powerNetwork) : null;

  if (renaming !== null && onRename) {
    return (
      <form
        style={{ display: "flex", gap: 4, alignItems: "center", flex: 1 }}
        onSubmit={(e) => {
          e.preventDefault();
          onRename(renaming);
          setRenaming(null);
        }}
      >
        <input
          autoFocus
          value={renaming}
          onChange={(e) => setRenaming(e.target.value)}
          onKeyDown={(e) => e.key === "Escape" && setRenaming(null)}
          style={{
            flex: 1,
            fontSize: 13,
            padding: "2px 6px",
            background: "#0f172a",
            border: "1px solid #334155",
            borderRadius: 4,
            color: "#e5e7eb",
            fontWeight: 600,
          }}
        />
        <button type="submit" style={{ fontSize: 11, padding: "2px 6px" }}>Save</button>
        <button type="button" onClick={() => setRenaming(null)} style={{ fontSize: 11, padding: "2px 6px" }}>✕</button>
      </form>
    );
  }

  return (
    <div style={{ display: "flex", alignItems: "center", gap: 6, flexWrap: "wrap" }}>
      <span style={{ fontWeight: 600, fontSize: compact ? 12 : 13 }}>{frame.name}</span>
      {onRename && (
        <button
          type="button"
          onClick={() => setRenaming(frame.name ?? "")}
          title="Rename frame"
          style={{ border: "none", background: "transparent", color: "#6b7280", cursor: "pointer", fontSize: 12, padding: "0 4px" }}
        >
          ✎
        </button>
      )}
      <Badge label={`#${frame.id}`} variant="id" />
      <Badge label={healthBadge} variant="state" />
      {netAvail !== null && (
        <Badge
          label={`⚡ ${netAvail.toFixed(2)}`}
          variant="power"
          color={netAvail >= 0 ? "#14532d" : "#7f1d1d"}
          title={`Available power: ${netAvail.toFixed(2)}`}
        />
      )}
    </div>
  );
}
