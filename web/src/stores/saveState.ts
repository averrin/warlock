import { create } from "zustand";
import type { RpcClient } from "../rpc/client";

interface SaveState {
  autosaveEnabled: boolean;
  autosaveInterval: number;
  lastSavedAt: string | null;
  saving: boolean;

  init: (client: RpcClient) => void;
  fetchStatus: (client: RpcClient) => Promise<void>;
  save: (client: RpcClient) => Promise<void>;
  load: (client: RpcClient) => Promise<void>;
  configureAutosave: (client: RpcClient, enabled: boolean, intervalSeconds: number) => Promise<void>;
}

export const useSaveStore = create<SaveState>((set, get) => ({
  autosaveEnabled: false,
  autosaveInterval: 60,
  lastSavedAt: null,
  saving: false,

  init: (client) => {
    client.on("notify.state.saved", (payload) => {
      const p = payload as { saved_at?: string; auto?: boolean };
      set({ lastSavedAt: p.saved_at ?? null, saving: false });
    });
  },

  fetchStatus: async (client) => {
    try {
      const result = await client.call<{
        enabled: boolean;
        interval_seconds: number;
        last_saved_at: string | null;
      }>("state.autosave.status");
      set({
        autosaveEnabled: result.enabled,
        autosaveInterval: result.interval_seconds,
        lastSavedAt: result.last_saved_at,
      });
    } catch {
      // endpoint may not exist yet
    }
  },

  save: async (client) => {
    set({ saving: true });
    try {
      await client.call("state.save");
    } catch {
      set({ saving: false });
    }
  },

  load: async (client) => {
    await client.call("state.load");
  },

  configureAutosave: async (client, enabled, intervalSeconds) => {
    const result = await client.call<{
      enabled: boolean;
      interval_seconds: number;
    }>("state.autosave.configure", {
      enabled,
      interval_seconds: intervalSeconds,
    });
    set({
      autosaveEnabled: result.enabled,
      autosaveInterval: result.interval_seconds,
    });
  },
}));
