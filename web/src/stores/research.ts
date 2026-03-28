import { create } from "zustand";
import type { RpcClient } from "../rpc/client";
import type { ResearchNodeDTO } from "../rpc/types";

interface ResearchStore {
  nodes: ResearchNodeDTO[];
  setNodes: (nodes: ResearchNodeDTO[]) => void;
  fetch: (client: RpcClient) => Promise<void>;
  unlock: (client: RpcClient, name: string) => Promise<void>;
  unlockAll: (client: RpcClient) => Promise<void>;
}

export const useResearchStore = create<ResearchStore>((set) => ({
  nodes: [],

  setNodes: (nodes) => set({ nodes }),

  fetch: async (client) => {
    const data = await client.call<{ nodes: ResearchNodeDTO[] }>("research.list");
    set({ nodes: data.nodes ?? [] });
  },

  unlock: async (client, name) => {
    const data = await client.call<{ nodes: ResearchNodeDTO[] }>("research.unlock", { name });
    set({ nodes: data.nodes ?? [] });
  },

  unlockAll: async (client) => {
    const data = await client.call<{ nodes: ResearchNodeDTO[] }>("research.unlock_all");
    set({ nodes: data.nodes ?? [] });
  },
}));
