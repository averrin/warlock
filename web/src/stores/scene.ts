import { create } from "zustand";
import type { RpcClient } from "../rpc/client";

interface SceneSnapshot {
  tick: number;
  texts: unknown[];
  lines: unknown[];
  sprites: unknown[];
  hitboxes: unknown[];
}

interface SceneStore {
  snapshot: SceneSnapshot;
  init: (client: RpcClient) => void;
  refresh: (client: RpcClient) => Promise<void>;
}

export const useSceneStore = create<SceneStore>((set) => ({
  snapshot: {
    tick: 0,
    texts: [],
    lines: [],
    sprites: [],
    hitboxes: [],
  },

  init: (client) => {
    client.on("notify.state.changed", async () => {
      await useSceneStore.getState().refresh(client);
    });
  },

  refresh: async (client) => {
    const snapshot = (await client.call("state.snapshot")) as SceneSnapshot;
    set({ snapshot });
  },
}));
