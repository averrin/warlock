import type { CSSProperties, MouseEventHandler } from "react";

type Props = {
  icon: string;
  onClick?: MouseEventHandler<HTMLButtonElement>;
  disabled?: boolean;
  title?: string;
  variant?: "default" | "danger";
  size?: "sm" | "md";
};

export function IconButton({ icon, onClick, disabled, title, variant = "default", size = "md" }: Props) {
  const dim = size === "sm" ? 24 : 28;
  const fontSize = size === "sm" ? 11 : 12;

  const style: CSSProperties = {
    border: "1px solid #334155",
    borderRadius: 6,
    padding: 0,
    width: dim,
    height: dim,
    background: "#020617",
    color: variant === "danger" ? "#ef4444" : "#e5e7eb",
    cursor: disabled ? "not-allowed" : "pointer",
    fontSize,
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    opacity: disabled ? 0.5 : 1,
  };

  return (
    <button type="button" style={style} onClick={onClick} disabled={disabled} title={title}>
      {icon}
    </button>
  );
}
