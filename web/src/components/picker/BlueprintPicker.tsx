import { useEffect, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { Picker, type PickerItem } from "./Picker";

type BlueprintData = {
  name: string;
  size: string;
};

type Props = {
  isOpen: boolean;
  onClose: () => void;
  rpcClient: RpcClient;
  onSelect: (blueprint: string) => void;
};

export function BlueprintPicker({ isOpen, onClose, rpcClient, onSelect }: Props) {
  const [blueprints, setBlueprints] = useState<PickerItem<BlueprintData>[]>([]);
  const [isLoading, setIsLoading] = useState(false);

  useEffect(() => {
    if (!isOpen) return;
    setIsLoading(true);
    void rpcClient
      .call<{ blueprints: Record<string, string> }>("code.blueprints")
      .then((data) => {
        const entries = Object.entries(data.blueprints ?? {});
        setBlueprints(
          entries
            .map(([name, src]) => {
              const size = src.match(/FrameSize\.(\w+)/)?.[1] ?? "M";
              return {
                id: name,
                label: `${name} (${size})`,
                data: { name, size },
              };
            })
            .sort((a, b) => a.data.name.localeCompare(b.data.name))
        );
      })
      .catch(() => setBlueprints([]))
      .finally(() => setIsLoading(false));
  }, [rpcClient, isOpen]);

  const handleSelect = (data: BlueprintData) => {
    onSelect(data.name);
    onClose();
  };

  return (
    <Picker
      isOpen={isOpen}
      onClose={onClose}
      items={blueprints}
      isLoading={isLoading}
      onSelect={handleSelect}
      placeholder="Search blueprints..."
      heading="Blueprints"
    />
  );
}
