import { create } from "zustand";
import type { RpcClient } from "../rpc/client";
import type { SessionInfo } from "../rpc/types";

type ConnectionState = "disconnected" | "connecting" | "connected" | "error";

interface ConnectionStore {
  status: ConnectionState;
  claimed: boolean;
  sessionInfo: SessionInfo | null;
  init: (client: RpcClient) => void;
  refreshSessionInfo: (client: RpcClient) => Promise<void>;
  claim: (client: RpcClient) => Promise<void>;
}

export const useConnectionStore = create<ConnectionStore>((set) => ({
  status: "disconnected",
  claimed: false,
  sessionInfo: null,

  init: (client) => {
    set({ status: "connecting" });
    client.onLifecycle("connected", () => {
      set({ status: "connected" });
      void useConnectionStore.getState().refreshSessionInfo(client);
      // Claim can race startup; retry briefly so web actions are not stuck read-only.
      void (async () => {
        for (let attempt = 0; attempt < 5; attempt += 1) {
          try {
            await useConnectionStore.getState().claim(client);
            return;
          } catch {
            await new Promise((resolve) => setTimeout(resolve, 250));
          }
        }
      })();
    });
    client.onLifecycle("disconnected", () => {
      set({ status: "disconnected", claimed: false });
    });
    client.onLifecycle("error", () => {
      set({ status: "error" });
    });
  },

  refreshSessionInfo: async (client) => {
    const sessionInfo = (await client.call("session.info")) as SessionInfo;
    set({ sessionInfo });
  },

  claim: async (client) => {
    await client.call("session.claim");
    set({ claimed: true });
  },
}));
