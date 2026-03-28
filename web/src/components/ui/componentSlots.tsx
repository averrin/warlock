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

const SLOT_COLORS: Record<string, string> = { S: "#1e3a5f", M: "#1a3a2a", L: "#3a1a1a" };
const SLOT_FULL_COLORS: Record<string, string> = { S: "#374151", M: "#374151", L: "#374151" };

export function ComponentSlotsRow({ frame }: { frame: FrameDTO }) {
  const s = frame.component_slots;
  if (!s) return null;
  const badges = ORDER.filter((k) => s[k] && s[k]!.max > 0);
  if (!badges.length) return null;
  return (
    <div
      style={{ display: "flex", alignItems: "center", gap: 4, marginBottom: 6 }}
      title="Component size slots (used / max)"
    >
      {badges.map((k) => {
        const u = s[k]!;
        const free = u.max - u.used;
        const full = free === 0;
        const bg = full ? SLOT_FULL_COLORS[k] : SLOT_COLORS[k];
        return (
          <span
            key={k}
            style={{
              fontSize: 10,
              borderRadius: 4,
              padding: "1px 5px",
              background: bg,
              color: full ? "#6b7280" : "#94a3b8",
              border: "1px solid #1f2937",
              fontFamily: "ui-monospace, monospace",
            }}
            title={`${k}: ${u.used} used / ${u.max} max (${free} free)`}
          >
            {k} {u.used}/{u.max}
          </span>
        );
      })}
    </div>
  );
}
