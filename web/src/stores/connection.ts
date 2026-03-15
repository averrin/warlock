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
      void useConnectionStore.getState().claim(client);
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
