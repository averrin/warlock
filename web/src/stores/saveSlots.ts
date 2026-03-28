import { create } from "zustand";
import type { RpcClient } from "../rpc/client";

export interface SaveSlot {
  name: string;
  saved_at: string;
}

export interface InitState {
  name: string;
  builtin?: boolean;
}

export interface BackupEntry {
  name: string;
  saved_at: string;
}

interface SaveSlotsState {
  slots: SaveSlot[];
  initStates: InitState[];
  backups: BackupEntry[];
  openInitName: string | null;
  currentSlotName: string | null;

  fetchSlots: (client: RpcClient) => Promise<void>;
  fetchInitStates: (client: RpcClient) => Promise<void>;
  fetchBackups: (client: RpcClient) => Promise<void>;

  saveSlot: (client: RpcClient, name: string) => Promise<void>;
  loadSlot: (client: RpcClient, name: string) => Promise<void>;
  deleteSlot: (client: RpcClient, name: string) => Promise<void>;

  saveCurrentAsInit: (client: RpcClient, name: string) => Promise<void>;
  deleteInit: (client: RpcClient, name: string) => Promise<void>;
  openInit: (client: RpcClient, name: string) => Promise<void>;
  saveInitEdits: (client: RpcClient) => Promise<void>;
  closeInit: (client: RpcClient) => Promise<void>;

  restoreBackup: (client: RpcClient, name: string) => Promise<void>;

  init: (client: RpcClient) => void;
}

export const useSaveSlotsStore = create<SaveSlotsState>((set, get) => ({
  slots: [],
  initStates: [],
  backups: [],
  openInitName: null,
  currentSlotName: null,

  init: (client) => {
    client.on("notify.slots.changed", () => {
      void get().fetchSlots(client);
    });
    client.on("notify.init.changed", () => {
      void get().fetchInitStates(client);
    });
    client.on("notify.backups.changed", () => {
      void get().fetchBackups(client);
    });
  },

  fetchSlots: async (client) => {
    try {
      const res = await client.call<{ slots: SaveSlot[] }>("state.slots.list");
      set({ slots: res.slots ?? [] });
    } catch {
      // ignore
    }
  },

  fetchInitStates: async (client) => {
    try {
      const res = await client.call<{ inits: InitState[] }>("state.init.list");
      set({ initStates: res.inits ?? [] });
    } catch {
      // ignore
    }
  },

  fetchBackups: async (client) => {
    try {
      const res = await client.call<{ backups: BackupEntry[] }>("state.backups.list");
      set({ backups: res.backups ?? [] });
    } catch {
      // ignore
    }
  },

  saveSlot: async (client, name) => {
    await client.call("state.slots.save", { name });
    set({ currentSlotName: name });
    await get().fetchSlots(client);
  },

  loadSlot: async (client, name) => {
    await client.call("state.slots.load", { name });
    set({ currentSlotName: name });
  },

  deleteSlot: async (client, name) => {
    await client.call("state.slots.delete", { name });
    await get().fetchSlots(client);
  },

  saveCurrentAsInit: async (client, name) => {
    await client.call("state.init.save_current", { name });
    await get().fetchInitStates(client);
  },

  deleteInit: async (client, name) => {
    await client.call("state.init.delete", { name });
    await get().fetchInitStates(client);
  },

  openInit: async (client, name) => {
    await client.call("state.init.open", { name });
    set({ openInitName: name });
  },

  saveInitEdits: async (client) => {
    await client.call("state.init.save_edits");
  },

  closeInit: async (client) => {
    await client.call("state.init.close");
    set({ openInitName: null });
  },

  restoreBackup: async (client, name) => {
    await client.call("state.backups.restore", { name });
  },
}));
