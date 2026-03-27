import { useState, type CSSProperties } from "react";
import { STATE_COLORS } from "./constants";

type Props = {
  label: string;
  variant?: "state" | "id" | "power" | "custom";
  color?: string;
  title?: string;
  onClick?: () => void;
};

export function Badge({ label, variant = "state", color, title, onClick }: Props) {
  const [copied, setCopied] = useState(false);

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
    color: copied ? "#22c55e" : "#e5e7eb",
    display: "inline-flex",
    alignItems: "center",
    gap: 4,
    cursor: onClick ? "pointer" : undefined,
    userSelect: "none",
    transition: "color 0.15s",
  };

  const handleClick = onClick
    ? (e: React.MouseEvent) => {
        e.stopPropagation();
        onClick();
        setCopied(true);
        setTimeout(() => setCopied(false), 1000);
      }
    : undefined;

  return (
    <span style={style} title={copied ? "Copied!" : title} onClick={handleClick}>
      {copied ? "✓" : label}
    </span>
  );
}

/** Format a numeric id as hex with at least 2 digits, e.g. 10 → "0x0A", 45 → "0x2D" */
export function fmtId(id: number): string {
  return `0x${id.toString(16).toUpperCase().padStart(2, "0")}`;
}

/** Copy text to clipboard and return the text */
export function copyToClipboard(text: string): void {
  void navigator.clipboard.writeText(text).catch(() => {
    // fallback for non-secure contexts
    const el = document.createElement("textarea");
    el.value = text;
    document.body.appendChild(el);
    el.select();
    document.execCommand("copy");
    document.body.removeChild(el);
  });
}
