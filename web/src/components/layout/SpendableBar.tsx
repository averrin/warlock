import { useEffect, useMemo, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";

type ItemRow = { name: string; tier: number; spendable?: boolean };

const FALLBACK_ORDER: string[] = [
  "Frame Parts",
  "Electronic Parts",
  "Ultralight Structures",
  "Advanced Chips",
  "Next Gen Composits",
  "Holographic Chips",
];

function tierAccent(tier: number): string {
  if (tier >= 3) return "#a78bfa";
  if (tier === 2) return "#38bdf8";
  return "#94a3b8";
}

type Props = {
  rpcClient: RpcClient;
};

export function SpendableBar({ rpcClient }: Props) {
  const pool = useGameStore((s) => s.spendablePool);
  const started = useGameStore((s) => s.started);
  const [catalog, setCatalog] = useState<ItemRow[]>([]);

  useEffect(() => {
    rpcClient
      .call<{ items: ItemRow[] }>("items.list")
      .then((r) => {
        const spend = (r.items ?? []).filter((x) => x.spendable);
        setCatalog(
          spend.length
            ? spend.map((x) => ({ name: x.name, tier: x.tier ?? 1 }))
            : FALLBACK_ORDER.map((name, i) => ({ name, tier: i < 2 ? 1 : i < 4 ? 2 : 3 })),
        );
      })
      .catch(() => {
        setCatalog(FALLBACK_ORDER.map((name, i) => ({ name, tier: i < 2 ? 1 : i < 4 ? 2 : 3 })));
      });
  }, [rpcClient]);

  const ordered = useMemo(() => {
    const byName = new Map(catalog.map((c) => [c.name, c]));
    const out: ItemRow[] = [];
    for (const name of FALLBACK_ORDER) {
      const row = byName.get(name);
      if (row) out.push(row);
    }
    for (const c of catalog) {
      if (!out.some((x) => x.name === c.name)) out.push(c);
    }
    return out;
  }, [catalog]);

  if (!started || ordered.length === 0) return null;

  const maxVal = Math.max(1, ...ordered.map((r) => pool[r.name] ?? 0));

  return (
    <div
      style={{
        display: "flex",
        alignItems: "stretch",
        gap: 8,
        padding: "6px 12px",
        borderBottom: "1px solid #1f2937",
        background: "#0f172a",
        fontSize: 11,
        flexWrap: "wrap",
      }}
    >
      <span style={{ color: "#64748b", alignSelf: "center", marginRight: 4 }}>Spendable</span>
      {ordered.map((row) => {
        const n = pool[row.name] ?? 0;
        const accent = tierAccent(row.tier);
        return (
          <div
            key={row.name}
            title={`${row.name} (tier ${row.tier})`}
            style={{
              flex: "1 1 120px",
              minWidth: 100,
              maxWidth: 200,
              display: "flex",
              flexDirection: "column",
              gap: 2,
            }}
          >
            <div style={{ display: "flex", justifyContent: "space-between", gap: 6, color: "#cbd5e1" }}>
              <span style={{ overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>{row.name}</span>
              <span style={{ color: accent, fontVariantNumeric: "tabular-nums" }}>{n}</span>
            </div>
            <div
              style={{
                height: 4,
                borderRadius: 2,
                background: "#1e293b",
                overflow: "hidden",
              }}
            >
              <div
                style={{
                  height: "100%",
                  width: `${Math.min(100, (n / maxVal) * 100)}%`,
                  background: accent,
                  transition: "width 0.2s ease",
                }}
              />
            </div>
          </div>
        );
      })}
    </div>
  );
}
