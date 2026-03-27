import { useEffect, useCallback, useMemo, type CSSProperties } from "react";
import type { RpcClient } from "../../rpc/client";
import type { ResearchNodeDTO } from "../../rpc/types";
import { useResearchStore } from "../../stores/research";
import { Panel } from "./Panel";

type Props = {
  rpcClient: RpcClient;
};

const NODE_W = 190;
const NODE_H = 96;
const COL_GAP = 240;
const ROW_GAP = 116;
const PAD = 20;

function computeLayout(nodes: ResearchNodeDTO[]): Map<string, { x: number; y: number }> {
  // Assign column = max prereq column + 1
  const cols = new Map<string, number>();
  const nameSet = new Set(nodes.map((n) => n.name));

  const getCol = (name: string, visited = new Set<string>()): number => {
    if (cols.has(name)) return cols.get(name)!;
    if (visited.has(name)) return 0;
    visited.add(name);
    const node = nodes.find((n) => n.name === name);
    if (!node || node.requires.length === 0) {
      cols.set(name, 0);
      return 0;
    }
    const maxPrereqCol = Math.max(
      ...node.requires
        .filter((r) => nameSet.has(r))
        .map((r) => getCol(r, new Set(visited)))
    );
    const col = maxPrereqCol + 1;
    cols.set(name, col);
    return col;
  };

  nodes.forEach((n) => getCol(n.name));

  // Group by column and sort within column by name for stability
  const byCol = new Map<number, string[]>();
  for (const [name, col] of cols.entries()) {
    if (!byCol.has(col)) byCol.set(col, []);
    byCol.get(col)!.push(name);
  }
  for (const arr of byCol.values()) {
    arr.sort();
  }

  const positions = new Map<string, { x: number; y: number }>();
  for (const [col, names] of byCol.entries()) {
    names.forEach((name, row) => {
      positions.set(name, {
        x: PAD + col * COL_GAP,
        y: PAD + row * ROW_GAP,
      });
    });
  }
  return positions;
}

function statusColors(status: ResearchNodeDTO["status"]): {
  bg: string;
  border: string;
  text: string;
} {
  if (status === "unlocked") return { bg: "#14532d", border: "#22c55e", text: "#bbf7d0" };
  if (status === "available") return { bg: "#451a03", border: "#f97316", text: "#fed7aa" };
  return { bg: "#111827", border: "#374151", text: "#6b7280" };
}

function CostBadge({ cost }: { cost: Record<string, number> }) {
  const entries = Object.entries(cost);
  if (entries.length === 0) return <span style={{ color: "#4ade80", fontSize: 10 }}>Free</span>;
  return (
    <div style={{ display: "flex", flexWrap: "wrap", gap: 3 }}>
      {entries.map(([k, v]) => (
        <span
          key={k}
          style={{
            background: "#1f2937",
            border: "1px solid #374151",
            borderRadius: 3,
            padding: "1px 4px",
            fontSize: 10,
            color: "#d1d5db",
            whiteSpace: "nowrap",
          }}
        >
          {v} {k}
        </span>
      ))}
    </div>
  );
}

function ResearchNode({
  node,
  pos,
  onUnlock,
}: {
  node: ResearchNodeDTO;
  pos: { x: number; y: number };
  onUnlock: (name: string) => void;
}) {
  const { bg, border, text } = statusColors(node.status);
  const clickable = node.status === "available";

  const cardStyle: CSSProperties = {
    position: "absolute",
    left: pos.x,
    top: pos.y,
    width: NODE_W,
    height: NODE_H,
    background: bg,
    border: `1px solid ${border}`,
    borderRadius: 6,
    padding: "8px 10px",
    boxSizing: "border-box",
    cursor: clickable ? "pointer" : "default",
    display: "flex",
    flexDirection: "column",
    gap: 4,
    userSelect: "none",
    transition: "border-color 0.15s",
  };

  return (
    <div
      style={cardStyle}
      title={node.description + (node.unlocks.length ? `\nUnlocks: ${node.unlocks.join(", ")}` : "")}
      onClick={() => clickable && onUnlock(node.name)}
    >
      <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
        <span style={{ fontSize: 14, color: text, fontWeight: 700, flex: 1, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
          {node.status === "unlocked" ? "✓ " : node.status === "available" ? "○ " : "🔒 "}
          {node.name}
        </span>
      </div>
      <CostBadge cost={node.cost} />
      {node.unlocks.length > 0 && (
        <div style={{ fontSize: 10, color: "#6b7280", overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>
          {node.unlocks.slice(0, 3).join(", ")}
          {node.unlocks.length > 3 ? ` +${node.unlocks.length - 3}` : ""}
        </div>
      )}
    </div>
  );
}

function Edges({
  nodes,
  positions,
}: {
  nodes: ResearchNodeDTO[];
  positions: Map<string, { x: number; y: number }>;
}) {
  const nameToNode = useMemo(
    () => new Map(nodes.map((n) => [n.name, n])),
    [nodes]
  );

  const edges: { x1: number; y1: number; x2: number; y2: number; unlocked: boolean }[] = [];
  for (const node of nodes) {
    const toPos = positions.get(node.name);
    if (!toPos) continue;
    for (const req of node.requires) {
      const fromPos = positions.get(req);
      if (!fromPos) continue;
      const parentUnlocked = nameToNode.get(req)?.status === "unlocked";
      edges.push({
        x1: fromPos.x + NODE_W,
        y1: fromPos.y + NODE_H / 2,
        x2: toPos.x,
        y2: toPos.y + NODE_H / 2,
        unlocked: parentUnlocked,
      });
    }
  }

  if (edges.length === 0) return null;

  const maxX = Math.max(...edges.map((e) => Math.max(e.x1, e.x2))) + PAD;
  const maxY = Math.max(...edges.map((e) => Math.max(e.y1, e.y2))) + PAD;

  return (
    <svg
      style={{ position: "absolute", left: 0, top: 0, pointerEvents: "none" }}
      width={maxX}
      height={maxY}
    >
      {edges.map((e, i) => {
        const cx1 = e.x1 + (e.x2 - e.x1) * 0.5;
        const cx2 = e.x2 - (e.x2 - e.x1) * 0.5;
        return (
          <path
            key={i}
            d={`M${e.x1},${e.y1} C${cx1},${e.y1} ${cx2},${e.y2} ${e.x2},${e.y2}`}
            fill="none"
            stroke={e.unlocked ? "#22c55e" : "#374151"}
            strokeWidth={1.5}
            strokeDasharray={e.unlocked ? undefined : "4 3"}
          />
        );
      })}
    </svg>
  );
}

const debugBtn: CSSProperties = {
  padding: "2px 8px",
  fontSize: 11,
  border: "1px solid #374151",
  borderRadius: 4,
  cursor: "pointer",
  background: "#1f2937",
  color: "#e5e7eb",
};

export function ResearchTree({ rpcClient }: Props) {
  const nodes = useResearchStore((s) => s.nodes);
  const fetch = useResearchStore((s) => s.fetch);
  const unlock = useResearchStore((s) => s.unlock);
  const unlockAll = useResearchStore((s) => s.unlockAll);

  useEffect(() => {
    void fetch(rpcClient);
  }, [rpcClient, fetch]);

  const positions = useMemo(() => computeLayout(nodes), [nodes]);

  const canvasW = useMemo(() => {
    let max = 400;
    for (const { x } of positions.values()) max = Math.max(max, x + NODE_W + PAD);
    return max;
  }, [positions]);

  const canvasH = useMemo(() => {
    let max = 200;
    for (const { y } of positions.values()) max = Math.max(max, y + NODE_H + PAD);
    return max;
  }, [positions]);

  const handleUnlock = useCallback(
    (name: string) => {
      void unlock(rpcClient, name);
    },
    [rpcClient, unlock]
  );

  const handleUnlockAll = useCallback(() => {
    void unlockAll(rpcClient);
  }, [rpcClient, unlockAll]);

  const stats = useMemo(() => {
    const total = nodes.length;
    const done = nodes.filter((n) => n.status === "unlocked").length;
    return { total, done };
  }, [nodes]);

  const header = (
    <div style={{ display: "flex", alignItems: "center", gap: 8, width: "100%" }}>
      <span style={{ flex: 1, fontWeight: 600, fontSize: 13 }}>
        Research Tree
        {stats.total > 0 && (
          <span style={{ color: "#6b7280", fontWeight: 400, fontSize: 11, marginLeft: 6 }}>
            {stats.done}/{stats.total}
          </span>
        )}
      </span>
      <button type="button" style={debugBtn} onClick={handleUnlockAll} title="Debug: unlock all research instantly">
        Unlock All
      </button>
    </div>
  );

  return (
    <Panel title="Research Tree" headerContent={header}>
      {nodes.length === 0 ? (
        <div style={{ color: "#6b7280", fontSize: 12, padding: 8 }}>Loading research…</div>
      ) : (
        <div style={{ position: "relative", width: canvasW, height: canvasH, minWidth: "100%" }}>
          <Edges nodes={nodes} positions={positions} />
          {nodes.map((node) => {
            const pos = positions.get(node.name);
            if (!pos) return null;
            return (
              <ResearchNode
                key={node.name}
                node={node}
                pos={pos}
                onUnlock={handleUnlock}
              />
            );
          })}
        </div>
      )}
    </Panel>
  );
}
