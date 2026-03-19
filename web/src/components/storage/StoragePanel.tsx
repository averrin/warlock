import { useEffect, useState } from "react";
import type { StorageDTO } from "../../rpc/types";
import type { RpcClient } from "../../rpc/client";
import { useItemsCatalogStore } from "../../stores/itemsCatalog";
import { useGameStore } from "../../stores/game";

type Props = {
  storage: StorageDTO;
  frameId: number;
  componentId: number;
  mode: "compact" | "full";
  rpcClient: RpcClient;
  onRefresh: () => Promise<void>;
};

/** Subscribes to store so storage updates re-render without parent redraw */
export function StoragePanelWithSubscription({
  frameId,
  componentId,
  mode,
  rpcClient,
}: Omit<Props, "storage" | "onRefresh">) {
  const storage = useGameStore((s) =>
    s.frames.find((f) => f.id === frameId)?.components?.find((c) => c.id === componentId)?.storage ?? null
  );
  const fetchInitialState = useGameStore((s) => s.fetchInitialState);
  if (!storage) return null;
  return (
    <StoragePanel
      storage={storage}
      frameId={frameId}
      componentId={componentId}
      mode={mode}
      rpcClient={rpcClient}
      onRefresh={async () => fetchInitialState(rpcClient)}
    />
  );
}

export function StoragePanel({ storage, frameId, componentId, mode, rpcClient, onRefresh }: Props) {
  const items = useItemsCatalogStore((s) => s.items);
  const fetchItems = useItemsCatalogStore((s) => s.fetchItems);
  const storageAddSlot = useGameStore((s) => s.storageAddSlot);
  const storageSetSlot = useGameStore((s) => s.storageSetSlot);
  const storageSetAmount = useGameStore((s) => s.storageSetAmount);
  const storageClearSlot = useGameStore((s) => s.storageClearSlot);

  const used = storage.slots_used ?? storage.slots.filter((s) => s.stack).length;
  const total = storage.slots_total ?? storage.slots_count ?? storage.slots.length;
  const topItems = storage.top_items ?? [];

  if (mode === "compact") {
    return (
      <div data-storage-panel="compact" style={{ fontSize: 11, color: "#9ca3af" }}>
        <span>📦 {used}/{total}</span>
        {topItems.length > 0 && (
          <span style={{ marginLeft: 6 }}>
            {topItems.map((t) => t.name).join(", ")}
          </span>
        )}
      </div>
    );
  }

  const handleAddSlot = async () => {
    await storageAddSlot(rpcClient, frameId, componentId);
    await onRefresh();
  };

  const handleSetSlot = async (slotId: number, itemId: string, amount: number) => {
    await storageSetSlot(rpcClient, frameId, componentId, slotId, itemId, amount);
    await onRefresh();
  };

  const handleSetAmount = async (slotId: number, amount: number) => {
    if (amount <= 0) {
      await storageClearSlot(rpcClient, frameId, componentId, slotId);
    } else {
      await storageSetAmount(rpcClient, frameId, componentId, slotId, amount);
    }
    await onRefresh();
  };

  const handleClear = async (slotId: number) => {
    await storageClearSlot(rpcClient, frameId, componentId, slotId);
    await onRefresh();
  };

  return (
    <div data-storage-panel="full" style={{ display: "grid", gap: 4 }}>
      <div style={{ fontSize: 11, fontWeight: 600, color: "#9ca3af" }}>
        Storage ({total} slots)
      </div>
      {storage.slots.map((slot) => (
        <SlotRow
          key={slot.id}
          slot={slot}
          items={items}
          onFetchItems={() => void fetchItems(rpcClient)}
          onAdd={(itemId, amount) => void handleSetSlot(slot.id, itemId, amount)}
          onSetAmount={(amount) => void handleSetAmount(slot.id, amount)}
          onClear={() => void handleClear(slot.id)}
        />
      ))}
      <button
        type="button"
        onClick={() => void handleAddSlot()}
        style={{ fontSize: 10, padding: "2px 6px", alignSelf: "flex-start" }}
      >
        + Add slot
      </button>
    </div>
  );
}

function FilledSlotRow({
  slot,
  maxAmount,
  onSetAmount,
  onClear,
}: {
  slot: StorageDTO["slots"][0];
  maxAmount: number;
  onSetAmount: (amount: number) => void;
  onClear: () => void;
}) {
  const [draft, setDraft] = useState(String(slot.stack!.amount));
  const current = slot.stack!.amount;
  useEffect(() => {
    setDraft(String(current));
  }, [current, slot.stack?.item]);
  const hasChange = draft !== String(current);

  return (
    <div
      data-slot={slot.id}
      style={{ fontSize: 11, display: "flex", gap: 6, alignItems: "center", flexWrap: "wrap" }}
    >
      <span>{slot.stack!.item}</span>
      <input
        type="number"
        min={0}
        max={maxAmount}
        value={draft}
        onChange={(e) => setDraft(e.target.value)}
        style={{ width: 48 }}
      />
      <span>/ {maxAmount}</span>
      {hasChange && (
        <button
          type="button"
          onClick={() => {
            const v = parseInt(draft, 10);
            if (!isNaN(v) && v >= 0) onSetAmount(v);
          }}
          style={{ fontSize: 10, padding: "2px 6px" }}
        >
          Apply
        </button>
      )}
      <button type="button" onClick={() => void onClear()} style={{ fontSize: 10, padding: "2px 6px" }}>
        Drop
      </button>
    </div>
  );
}

function SlotRow({
  slot,
  items,
  onFetchItems,
  onAdd,
  onSetAmount,
  onClear,
}: {
  slot: StorageDTO["slots"][0];
  items: { id: string; name: string }[];
  onFetchItems: () => void;
  onAdd: (itemId: string, amount: number) => void;
  onSetAmount: (amount: number) => void;
  onClear: () => void;
}) {
  const [selectedItemId, setSelectedItemId] = useState("");
  const [amountDraft, setAmountDraft] = useState("1");

  const hasStack = slot.stack != null;
  const maxAmount = slot.stack?.max ?? 99;

  if (hasStack) {
    return (
      <FilledSlotRow
        slot={slot}
        maxAmount={maxAmount}
        onSetAmount={onSetAmount}
        onClear={onClear}
      />
    );
  }

  return (
    <div
      key={slot.id}
      data-slot={slot.id}
      data-slot-empty="true"
      style={{ fontSize: 11, display: "flex", gap: 6, alignItems: "center", flexWrap: "wrap" }}
    >
      <select
        value={selectedItemId}
        onChange={(e) => setSelectedItemId(e.target.value)}
        onFocus={() => void onFetchItems()}
        style={{ minWidth: 80 }}
      >
        <option value="">Select item</option>
        {items.map((i) => (
          <option key={i.id} value={i.id}>
            {i.name}
          </option>
        ))}
      </select>
      <input
        type="number"
        min={1}
        max={999}
        value={amountDraft}
        onChange={(e) => setAmountDraft(e.target.value)}
        style={{ width: 48 }}
      />
      <button
        type="button"
        disabled={!selectedItemId}
        onClick={() => {
          const amt = parseInt(amountDraft, 10);
          if (selectedItemId && !isNaN(amt) && amt >= 1) onAdd(selectedItemId, amt);
        }}
        style={{ fontSize: 10, padding: "2px 6px" }}
      >
        Add
      </button>
    </div>
  );
}
