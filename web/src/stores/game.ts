import { create } from "zustand";
import type { RpcClient } from "../rpc/client";
import type { ConnectionDTO, EnvironmentDTO, FrameDTO, PowerNetworkDTO } from "../rpc/types";

interface GameStore {
  started: boolean;
  frames: FrameDTO[];
  connections: ConnectionDTO[];
  environment: EnvironmentDTO | null;
  powerNetworks: PowerNetworkDTO[];
  selectedFrameId: number | null;
  init: (client: RpcClient) => void;
  fetchInitialState: (client: RpcClient) => Promise<void>;
  selectFrame: (frameId: number | null) => void;
  createFrameAt: (client: RpcClient, name: string, x: number, y: number) => Promise<void>;
  moveFrame: (client: RpcClient, frameId: number, x: number, y: number) => Promise<void>;
  activateFrame: (client: RpcClient, frameId: number) => Promise<void>;
  createFromBlueprint: (client: RpcClient, blueprint: string) => Promise<void>;
  addComponent: (client: RpcClient, frameId: number, componentName: string) => Promise<void>;
  updateCoreCode: (client: RpcClient, frameId: number, code: string) => Promise<void>;
}

export const useGameStore = create<GameStore>((set, get) => ({
  started: false,
  frames: [],
  connections: [],
  environment: null,
  powerNetworks: [],
  selectedFrameId: null,

  init: (client) => {
    client.on("notify.state.changed", async () => {
      await get().fetchInitialState(client);
    });
    client.on("notify.game.started", async () => {
      await get().fetchInitialState(client);
    });
  },

  fetchInitialState: async (client) => {
    const [state, power] = await Promise.all([
      client.call("game.state"),
      client.call("power.networks"),
    ]);
    const casted = state as {
      started: boolean;
      frames: FrameDTO[];
      connections: ConnectionDTO[];
      environment: EnvironmentDTO | null;
    };
    const powerCasted = power as { networks: PowerNetworkDTO[] };
    set({
      started: casted.started,
      frames: casted.frames ?? [],
      connections: casted.connections ?? [],
      environment: casted.environment ?? null,
      powerNetworks: powerCasted.networks ?? [],
    });
  },

  selectFrame: (frameId) => set({ selectedFrameId: frameId }),

  createFrameAt: async (client, name, x, y) => {
    await client.call("frame.create", { name, position: { x, y } });
    await get().fetchInitialState(client);
  },

  moveFrame: async (client, frameId, x, y) => {
    await client.call("frame.move", { id: frameId, position: { x, y } });
    set({
      frames: get().frames.map((frame) =>
        frame.id === frameId ? { ...frame, position: { x, y } } : frame,
      ),
    });
  },

  activateFrame: async (client, frameId) => {
    await client.call("frame.activate", { id: frameId });
  },

  createFromBlueprint: async (client, blueprint) => {
    await client.call("frame.create_from_blueprint", { blueprint });
    await get().fetchInitialState(client);
  },

  addComponent: async (client, frameId, componentName) => {
    await client.call("component.add", { frame_id: frameId, component_name: componentName });
    await get().fetchInitialState(client);
  },

  updateCoreCode: async (client, frameId, code) => {
    await client.call("code.update", { frame_id: frameId, code });
  },
}));
