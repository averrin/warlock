import { useEffect, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { Picker, type PickerItem } from "./Picker";

type Props = {
  isOpen: boolean;
  onClose: () => void;
  frameId: number;
  rpcClient: RpcClient;
};

export function ComponentPicker({ isOpen, onClose, frameId, rpcClient }: Props) {
  const [components, setComponents] = useState<PickerItem<string>[]>([]);
  const [isLoading, setIsLoading] = useState(false);
  const addComponent = useGameStore((s) => s.addComponent);

  useEffect(() => {
    if (!isOpen) return;
    setIsLoading(true);
    void rpcClient
      .call<{ sources: Record<string, string> }>("code.sources")
      .then((data) => {
        const names = Object.keys(data.sources ?? {}).sort();
        setComponents(
          names.map((name) => ({
            id: name,
            label: name,
            data: name,
          }))
        );
      })
      .catch(() => setComponents([]))
      .finally(() => setIsLoading(false));
  }, [rpcClient, isOpen]);

  const handleSelect = (componentName: string) => {
    void addComponent(rpcClient, frameId, componentName);
    onClose();
  };

  return (
    <Picker
      isOpen={isOpen}
      onClose={onClose}
      items={components}
      isLoading={isLoading}
      onSelect={handleSelect}
      placeholder="Search components..."
      heading="Components"
    />
  );
}
