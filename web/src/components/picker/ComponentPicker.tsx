import { useEffect, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import {
  canAfford,
  formatCostLine,
  parseLuaSpendableCost,
  type SpendableCost,
} from "../../game/economy";
import { useGameStore } from "../../stores/game";
import { CostBadges } from "./CostBadges";
import { Picker, type PickerItem, type PickerSection } from "./Picker";

const CATEGORY_ORDER = ["Core", "Power", "Thermal", "Production", "Storage", "Network", "Connectors", "Other"];

type Props = {
  isOpen: boolean;
  onClose: () => void;
  frameId: number;
  rpcClient: RpcClient;
};

export function ComponentPicker({ isOpen, onClose, frameId, rpcClient }: Props) {
  const [sections, setSections] = useState<PickerSection<string>[]>([]);
  const [sourceMap, setSourceMap] = useState<Record<string, string>>({});
  const [isLoading, setIsLoading] = useState(false);
  const addComponent = useGameStore((s) => s.addComponent);
  const spendablePool = useGameStore((s) => s.spendablePool);

  useEffect(() => {
    if (!isOpen) return;
    setIsLoading(true);
    void rpcClient
      .call<{ sources: Record<string, string>; categories: Record<string, string> }>("code.sources")
      .then((data) => {
        setSourceMap(data.sources ?? {});
        const names = Object.keys(data.sources ?? {}).sort();
        const categoryMap: Record<string, string[]> = {};
        for (const name of names) {
          const cat = data.categories?.[name] ?? "Other";
          if (!categoryMap[cat]) categoryMap[cat] = [];
          categoryMap[cat].push(name);
        }
        const ordered = CATEGORY_ORDER.filter((c) => categoryMap[c]);
        const rest = Object.keys(categoryMap).filter((c) => !CATEGORY_ORDER.includes(c)).sort();
        setSections(
          [...ordered, ...rest].map((cat) => ({
            id: cat,
            heading: cat,
            items: categoryMap[cat].map((name) => {
              const src = data.sources?.[name] ?? "";
              const cost: SpendableCost = parseLuaSpendableCost(src);
              const line = formatCostLine(cost);
              const ok = canAfford(spendablePool, cost);
              const item: PickerItem<string> = {
                id: name,
                label: `${name} ${line}`,
                keywords: [name, line],
                showType: false,
                disabled: !ok,
                className: ok ? undefined : "picker-row-unaffordable",
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
                    <span style={{ fontWeight: 500, color: "#f8fafc" }}>{name}</span>
                    <span style={{ color: "#475569" }}>—</span>
                    <CostBadges cost={cost} pool={spendablePool} />
                    {!ok && (
                      <span style={{ fontSize: 11, color: "#f87171", fontWeight: 600 }}>
                        Can't afford
                      </span>
                    )}
                  </div>
                ),
                data: name,
              };
              return item;
            }),
          }))
        );
      })
      .catch(() => setSections([]))
      .finally(() => setIsLoading(false));
  }, [rpcClient, isOpen, spendablePool]);

  const handleSelect = (componentName: string) => {
    const cost = parseLuaSpendableCost(sourceMap[componentName] ?? "");
    if (!canAfford(spendablePool, cost)) return;
    void addComponent(rpcClient, frameId, componentName);
    onClose();
  };

  return (
    <Picker
      isOpen={isOpen}
      onClose={onClose}
      sections={sections}
      isLoading={isLoading}
      onSelect={handleSelect}
      placeholder="Search components..."
    />
  );
}
