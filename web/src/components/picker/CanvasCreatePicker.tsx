import { useEffect, useMemo, useState } from "react";
import {
  blueprintTotalCost,
  canAfford,
  formatCostLine,
  type SpendableCost,
} from "../../game/economy";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { CostBadges } from "./CostBadges";
import { Picker, type PickerItem, type PickerSection } from "./Picker";
import type { PatchType } from "../../stores/patches";

export type CanvasCreatePick =
  | { kind: "frame"; blueprint: string; frameSizeKey: string; affordable: boolean }
  | { kind: "patch"; patchType: string }
  | { kind: "surface" }
  | { kind: "marker" };

type BlueprintRow = {
  name: string;
  size: string;
  cost: SpendableCost;
  costLine: string;
  affordable: boolean;
};

type Props = {
  isOpen: boolean;
  onClose: () => void;
  rpcClient: RpcClient;
  patchTypes: PatchType[];
  framesEnabled: boolean;
  onSelect: (pick: CanvasCreatePick) => void;
};

export function CanvasCreatePicker({
  isOpen,
  onClose,
  rpcClient,
  patchTypes,
  framesEnabled,
  onSelect,
}: Props) {
  const [frameRows, setFrameRows] = useState<BlueprintRow[]>([]);
  const spendablePool = useGameStore((s) => s.spendablePool);

  useEffect(() => {
    if (!isOpen || !framesEnabled) {
      setFrameRows([]);
      return;
    }
    void Promise.all([
      rpcClient.call<{ blueprints: Record<string, string> }>("code.blueprints"),
      rpcClient.call<{ sources: Record<string, string> }>("code.sources"),
    ])
      .then(([bpData, srcData]) => {
        const sources = srcData.sources ?? {};
        const bps = bpData.blueprints ?? {};
        const rows: BlueprintRow[] = Object.entries(bps)
          .map(([name, src]) => {
            const total = blueprintTotalCost(src, sources);
            return {
              name,
              size: src.match(/FrameSize\.(\w+)/)?.[1] ?? "M",
              cost: total,
              costLine: formatCostLine(total),
              affordable: canAfford(spendablePool, total),
            };
          })
          .sort((a, b) => a.name.localeCompare(b.name));
        setFrameRows(rows);
      })
      .catch(() => setFrameRows([]));
  }, [isOpen, framesEnabled, rpcClient, spendablePool]);

  const sections = useMemo((): PickerSection<CanvasCreatePick>[] => {
    const out: PickerSection<CanvasCreatePick>[] = [];
    if (framesEnabled && frameRows.length > 0) {
      const frameItems: PickerItem<CanvasCreatePick>[] = frameRows.map((row) => ({
        id: `frame-${row.name}`,
        label: `${row.name} (${row.size}) ${row.costLine}`,
        keywords: [row.name, row.size, row.costLine],
        showType: false,
        disabled: !row.affordable,
        className: row.affordable ? undefined : "picker-row-unaffordable",
        content: (
          <div
            style={{
              display: "flex",
              flex: 1,
              minWidth: 0,
              flexWrap: "wrap",
              alignItems: "center",
              gap: "6px 10px",
            }}
          >
            <span style={{ fontWeight: 500, color: "#f8fafc" }}>{row.name}</span>
            <span style={{ color: "#64748b", fontSize: 13 }}>({row.size})</span>
            <span style={{ color: "#475569" }}>—</span>
            <CostBadges cost={row.cost} pool={spendablePool} />
            {!row.affordable && (
              <span style={{ fontSize: 11, color: "#f87171", fontWeight: 600 }}>Can't afford</span>
            )}
          </div>
        ),
        data: {
          kind: "frame" as const,
          blueprint: row.name,
          frameSizeKey: row.size,
          affordable: row.affordable,
        },
      }));
      out.push({ id: "frames", heading: "Frames", items: frameItems });
    }
    out.push({
      id: "surface",
      heading: "Surface",
      items: [{ id: "surface", label: "Surface paint…", data: { kind: "surface" as const } }],
    });
    out.push({
      id: "markers",
      heading: "Markers",
      items: [{ id: "marker", label: "Marker…", data: { kind: "marker" as const } }],
    });
    if (patchTypes.length > 0) {
      const sorted = [...patchTypes].sort((a, b) => a.name.localeCompare(b.name));
      out.push({
        id: "patches",
        heading: "Resource patches",
        items: sorted.map((pt) => ({
          id: `patch-${pt.key}`,
          label: pt.name,
          data: { kind: "patch" as const, patchType: pt.key },
        })),
      });
    }
    return out;
  }, [frameRows, framesEnabled, patchTypes, spendablePool]);

  return (
    <Picker
      isOpen={isOpen}
      onClose={onClose}
      sections={sections}
      onSelect={onSelect}
      placeholder="Search create options…"
      heading="Create on canvas"
    />
  );
}
