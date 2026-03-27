import type { SpendableCost } from "../../game/economy";

type Props = {
  cost: SpendableCost;
  pool: Record<string, number>;
};

export function CostBadges({ cost, pool }: Props) {
  const entries = Object.entries(cost).filter(([, v]) => v > 0);
  if (entries.length === 0) {
    return <span style={{ color: "#64748b", fontSize: 12 }}>—</span>;
  }
  return (
    <span
      style={{
        display: "inline-flex",
        flexWrap: "wrap",
        gap: 6,
        alignItems: "center",
      }}
    >
      {entries.map(([name, amt]) => {
        const have = pool[name] ?? 0;
        const ok = have >= amt;
        return (
          <span
            key={name}
            title={`${have} / ${amt}`}
            style={{
              fontSize: 11,
              lineHeight: 1.35,
              padding: "2px 8px",
              borderRadius: 999,
              background: ok ? "#334155" : "rgba(127, 29, 29, 0.5)",
              color: ok ? "#e2e8f0" : "#fecaca",
              border: ok ? "1px solid transparent" : "1px solid rgba(185, 28, 28, 0.85)",
              whiteSpace: "nowrap",
            }}
          >
            <span style={{ fontWeight: 600 }}>{amt}</span> {name}
          </span>
        );
      })}
    </span>
  );
}
