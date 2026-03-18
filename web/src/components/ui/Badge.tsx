import type { CSSProperties } from "react";
import { STATE_COLORS } from "./constants";

type Props = {
  label: string;
  variant?: "state" | "id" | "power" | "custom";
  color?: string;
  title?: string;
};

export function Badge({ label, variant = "state", color, title }: Props) {
  let background = color;
  if (!background) {
    if (variant === "state") {
      background = STATE_COLORS[label.toUpperCase()] ?? STATE_COLORS.DEACTIVATED;
    } else if (variant === "id") {
      background = "#0b1120";
    } else if (variant === "power") {
      background = "#14532d";
    } else {
      background = "#1f2937";
    }
  }

  const style: CSSProperties = {
    fontSize: 10,
    borderRadius: 999,
    padding: "2px 6px",
    background,
    color: "#e5e7eb",
    display: "inline-flex",
    alignItems: "center",
    gap: 4,
  };

  return <span style={style} title={title}>{label}</span>;
}
