import { create } from "zustand";
import type { RpcClient } from "../rpc/client";
import type { ItemCatalogItemDTO } from "../rpc/types";
import { listItems } from "../rpc/items";

interface ItemsCatalogStore {
  items: ItemCatalogItemDTO[];
  itemsById: Record<string, ItemCatalogItemDTO>;
  loaded: boolean;
  loading: boolean;
  fetchItems: (client: RpcClient) => Promise<void>;
  reset: () => void;
}

export const useItemsCatalogStore = create<ItemsCatalogStore>((set, get) => ({
  items: [],
  itemsById: {},
  loaded: false,
  loading: false,

  fetchItems: async (client) => {
    const { loaded, loading } = get();
    if (loaded || loading) return;

    set({ loading: true });
    try {
      const result = await listItems(client);
      const items = result.items ?? [];
      const itemsById: Record<string, ItemCatalogItemDTO> = {};
      for (const item of items) {
        if (!item?.id) continue;
        itemsById[item.id] = item;
      }
      set({ items, itemsById, loaded: true, loading: false });
    } catch {
      set({ loading: false });
    }
  },

  reset: () => set({ items: [], itemsById: {}, loaded: false, loading: false }),
}));

