import type { FrameDTO } from "../../rpc/types";

const ORDER = ["S", "M", "L"] as const;

export function formatComponentSlotsLine(frame: FrameDTO): string | null {
  const s = frame.component_slots;
  if (!s) return null;
  const parts: string[] = [];
  for (const k of ORDER) {
    const u = s[k];
    if (!u || u.max === 0) continue;
    parts.push(`${k} ${u.used}/${u.max}`);
  }
  return parts.length ? parts.join("  ") : null;
}

export function componentSizeOptionsAllowed(
  slots: FrameDTO["component_slots"],
  currentSize: string,
  all: readonly string[],
): string[] {
  if (!slots) return [...all];
  const cur = (currentSize || "S").toUpperCase();
  return all.filter((opt) => {
    const o = opt.toUpperCase();
    if (o === cur) return true;
    for (const k of ORDER) {
      let used = slots[k]?.used ?? 0;
      if (k === cur) used--;
      if (k === o) used++;
      const max = slots[k]?.max ?? 0;
      if (used > max) return false;
    }
    return true;
  });
}

export function ComponentSlotsRow({ frame }: { frame: FrameDTO }) {
  const line = formatComponentSlotsLine(frame);
  if (!line) return null;
  return (
    <div
      style={{ fontSize: 11, color: "#94a3b8", marginBottom: 6 }}
      title="Component size slots (used / max per S, M, L)"
    >
      <span style={{ color: "#64748b" }}>Slots </span>
      {line}
    </div>
  );
}
