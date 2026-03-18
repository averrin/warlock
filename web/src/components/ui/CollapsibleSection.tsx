import { useState, type ReactNode } from "react";

type Props = {
  title: string;
  defaultOpen?: boolean;
  children: ReactNode;
};

export function CollapsibleSection({ title, defaultOpen = true, children }: Props) {
  const [open, setOpen] = useState(defaultOpen);

  return (
    <div style={{ marginBottom: 4 }}>
      <div
        onClick={() => setOpen(!open)}
        style={{ cursor: "pointer", fontWeight: 600, padding: "2px 0", userSelect: "none" }}
      >
        {open ? "▾" : "▸"} {title}
      </div>
      {open && <div style={{ paddingLeft: 16 }}>{children}</div>}
    </div>
  );
}
