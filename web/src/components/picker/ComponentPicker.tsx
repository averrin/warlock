import { useEffect, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { canAfford, formatCostLine, parseLuaSpendableCost } from "../../game/economy";
import { useGameStore } from "../../stores/game";
import { Picker, type PickerSection } from "./Picker";

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
              const cost = parseLuaSpendableCost(src);
              const line = formatCostLine(cost);
              const ok = canAfford(spendablePool, cost);
              return {
                id: name,
                label: `${name} — ${line}${ok ? "" : " (insufficient)"}`,
                data: name,
              };
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
