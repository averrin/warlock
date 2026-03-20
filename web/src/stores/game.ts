import { create } from "zustand";
import type { RpcClient } from "../rpc/client";
import type { ConnectionDTO, EnvironmentDTO, FrameDTO, PowerNetworkDTO } from "../rpc/types";
import { usePatchStore, Patch } from "./patches";

interface TimeControl {
  paused: boolean;
  multiplier: number;
}

export interface GameMarker {
  x: number;
  y: number;
  label: string;
  color: string;
}

interface GameStore {
  started: boolean;
  hydrated: boolean;
  frames: FrameDTO[];
  connections: ConnectionDTO[];
  environment: EnvironmentDTO | null;
  powerNetworks: PowerNetworkDTO[];
  selectedFrameId: number | null;
  /** Multi-select (RTS-style); primary for inspector is `selectedFrameId`. */
  selectedFrameIds: number[];
  timeControl: TimeControl;
  markers: GameMarker[];
  indicators: Record<string, { label: string; value: string; color: string }>;
  environmentHistory: EnvironmentDTO["history"] | null;
  init: (client: RpcClient) => void;
  applySnapshot: (snapshot: {
    started?: boolean;
    frames?: FrameDTO[];
    connections?: ConnectionDTO[];
    environment?: EnvironmentDTO | null;
    power_networks?: PowerNetworkDTO[];
    powerNetworks?: PowerNetworkDTO[];
  }) => void;
  fetchInitialState: (client: RpcClient) => Promise<void>;
  selectFrame: (frameId: number | null) => void;
  setFrameSelection: (frameIds: number[]) => void;
  toggleFrameSelection: (frameId: number) => void;
  pauseGame: (client: RpcClient) => Promise<void>;
  resumeGame: (client: RpcClient) => Promise<void>;
  setSpeed: (client: RpcClient, multiplier: number) => Promise<void>;
  refreshEnvStatus: (client: RpcClient) => Promise<void>;
  refreshPowerNetworks: (client: RpcClient) => Promise<void>;
  fetchSpeedState: (client: RpcClient) => Promise<void>;
  createFrameAt: (client: RpcClient, name: string, x: number, y: number) => Promise<void>;
  moveFrame: (client: RpcClient, frameId: number, x: number, y: number) => Promise<void>;
  activateFrame: (client: RpcClient, frameId: number) => Promise<void>;
  deactivateFrame: (client: RpcClient, frameId: number) => Promise<void>;
  setFrameSize: (client: RpcClient, frameId: number, size: string) => Promise<void>;
  setFrameMaterial: (client: RpcClient, frameId: number, material: string) => Promise<void>;
  updateFrameMetadata: (
    client: RpcClient,
    frameId: number,
    updates: { name?: string; description?: string; icon?: string; attributes?: Record<string, string | number | boolean> },
  ) => Promise<void>;
  removeComponent: (client: RpcClient, frameId: number, componentId: number) => Promise<void>;
  setComponentSize: (
    client: RpcClient,
    frameId: number,
    componentId: number,
    size: string,
  ) => Promise<void>;
  setComponentMaterial: (
    client: RpcClient,
    frameId: number,
    componentId: number,
    material: string,
  ) => Promise<void>;
  createFromBlueprint: (client: RpcClient, blueprint: string, position?: { x: number; y: number }) => Promise<void>;
  addComponent: (client: RpcClient, frameId: number, componentName: string) => Promise<void>;
  setComponentState: (
    client: RpcClient,
    frameId: number,
    componentId: number,
    state: "active" | "inactive",
  ) => Promise<void>;
  repairComponent: (client: RpcClient, frameId: number, componentId: number) => Promise<void>;
  updateComponentAttribute: (
    client: RpcClient,
    frameId: number,
    componentId: number,
    key: string,
    value: string | number | boolean,
  ) => Promise<void>;
  storageAddSlot: (client: RpcClient, frameId: number, componentId: number) => Promise<void>;
  storageSetSlot: (
    client: RpcClient,
    frameId: number,
    componentId: number,
    slotId: number,
    itemId: string,
    amount: number,
  ) => Promise<void>;
  storageSetAmount: (
    client: RpcClient,
    frameId: number,
    componentId: number,
    slotId: number,
    amount: number,
  ) => Promise<void>;
  storageClearSlot: (
    client: RpcClient,
    frameId: number,
    componentId: number,
    slotId: number,
  ) => Promise<void>;
  loadCoreCode: (client: RpcClient, frameId: number) => Promise<string>;
  executeCoreUpdate: (
    client: RpcClient,
    frameId: number,
  ) => Promise<{ status: "ok" | "error"; error?: string }>;
  createConnection: (
    client: RpcClient,
    source: number,
    target: number,
    type: "POWER" | "DATA" | "CONVEYOR" | "POE",
    medium?: "WIRE" | "WIRELESS" | "BEAM",
  ) => Promise<void>;
  removeConnection: (client: RpcClient, id: number) => Promise<void>;
  updateCoreCode: (client: RpcClient, frameId: number, code: string) => Promise<void>;
  removeFrame: (client: RpcClient, frameId: number) => Promise<void>;
  setEnvironmentField: (client: RpcClient, field: string, value: number) => Promise<void>;
}

export const useGameStore = create<GameStore>((set, get) => ({
  started: false,
  hydrated: false,
  frames: [],
  connections: [],
  environment: null,
  powerNetworks: [],
  selectedFrameId: null,
  selectedFrameIds: [],
  timeControl: { paused: false, multiplier: 1 },
  markers: [],
  indicators: {},
  environmentHistory: null,

  init: (client) => {
    client.onLifecycle("disconnected", () => {
      set({ hydrated: false });
    });
    client.on("event.state_update", (payload) => {
      const next = payload as { frames?: FrameDTO[]; connections?: ConnectionDTO[] };
      set((state) => {
        const frames = next.frames ?? state.frames;
        const ids = new Set(frames.map((f) => f.id));
        const nextSel = state.selectedFrameIds.filter((id) => ids.has(id));
        let primary = state.selectedFrameId;
        if (primary !== null && !ids.has(primary)) {
          primary = nextSel.length ? nextSel[nextSel.length - 1]! : null;
        }
        return {
          frames,
          connections: next.connections ?? state.connections,
          selectedFrameIds: nextSel,
          selectedFrameId: primary,
        };
      });
    });
    client.on("event.env_update", (payload) => {
      const next = payload as { env?: EnvironmentDTO };
      if (!next.env) return;
      set({
        environment: next.env,
        environmentHistory: next.env.history ?? null,
      });
    });
    client.on("event.power_update", (payload) => {
      const next = payload as { networks?: PowerNetworkDTO[] };
      set({
        powerNetworks: next.networks ?? [],
      });
    });
    client.on("notify.state.changed", async () => {
      await get().fetchInitialState(client);
    });
    client.on("notify.game.started", async () => {
      await get().fetchInitialState(client);
    });

    client.on("nexus.markers", (payload: any) => {
      if (payload.action === "clear") {
        set({ markers: [] });
      } else if (payload.action === "set" && payload.marker) {
        set((state) => ({
          markers: [
            ...state.markers.filter((m) => m.label !== payload.marker.label),
            payload.marker
          ]
        }));
      } else if (payload.action === "remove" && payload.label) {
        set((state) => ({ markers: state.markers.filter((m) => m.label !== payload.label) }));
      }
    });

    client.on("nexus.indicators", (payload: any) => {
      set((state) => ({
        indicators: {
          ...state.indicators,
          [payload.key]: {
            label: payload.label,
            value: payload.value,
            color: payload.color,
          },
        },
      }));
    });
  },

  applySnapshot: (snapshot) => {
    const nextFrames = snapshot.frames ?? [];
    const nextEnv = snapshot.environment ?? null;
    const nextPower = snapshot.power_networks ?? snapshot.powerNetworks ?? [];
    set((state) => {
      const ids = new Set(nextFrames.map((f) => f.id));
      const nextSel = state.selectedFrameIds.filter((id) => ids.has(id));
      let primary = state.selectedFrameId;
      if (primary !== null && !ids.has(primary)) {
        primary = nextSel.length ? nextSel[nextSel.length - 1]! : null;
      }
      return {
        started: snapshot.started ?? state.started,
        hydrated: true,
        frames: nextFrames,
        connections: snapshot.connections ?? [],
        environment: nextEnv,
        environmentHistory: nextEnv?.history ?? null,
        powerNetworks: nextPower,
        selectedFrameIds: nextSel,
        selectedFrameId: primary,
      };
    });
  },

  fetchInitialState: async (client) => {
    const snapshot = (await client.call("state.snapshot")) as {
      started?: boolean;
      frames?: FrameDTO[];
      connections?: ConnectionDTO[];
      environment?: EnvironmentDTO | null;
      power_networks?: PowerNetworkDTO[];
      patches?: Patch[];
    };
    get().applySnapshot(snapshot);
    if (snapshot.patches) {
      usePatchStore.getState().setPatches(snapshot.patches);
    }
  },

  selectFrame: (frameId) =>
    set({
      selectedFrameId: frameId,
      selectedFrameIds: frameId == null ? [] : [frameId],
    }),

  setFrameSelection: (frameIds) =>
    set({
      selectedFrameIds: frameIds,
      selectedFrameId: frameIds.length ? frameIds[frameIds.length - 1]! : null,
    }),

  toggleFrameSelection: (frameId) =>
    set((state) => {
      const had = state.selectedFrameIds.includes(frameId);
      const next = had
        ? state.selectedFrameIds.filter((id) => id !== frameId)
        : [...state.selectedFrameIds, frameId];
      let primary = state.selectedFrameId;
      if (had && frameId === primary) {
        primary = next.length ? next[next.length - 1]! : null;
      } else if (!had) {
        primary = frameId;
      }
      return { selectedFrameIds: next, selectedFrameId: primary };
    }),

  createFrameAt: async (client, name, x, y) => {
    await client.call("frame.create", { name, position: { x, y } });
    await get().fetchInitialState(client);
  },

  moveFrame: async (client, frameId, x, y) => {
    const frame = get().frames.find((f) => f.id === frameId);
    const entityId = frame?.entity_id ?? frameId;
    await client.call("frame.move", { id: entityId, position: { x, y } });
    set({
      frames: get().frames.map((f) =>
        f.id === frameId ? { ...f, position: { x, y } } : f,
      ),
    });
  },

  activateFrame: async (client, frameId) => {
    const frame = get().frames.find((f) => f.id === frameId);
    const entityId = frame?.entity_id ?? frameId;
    await client.call("frame.activate", { id: entityId });
  },

  deactivateFrame: async (client, frameId) => {
    const frame = get().frames.find((f) => f.id === frameId);
    const entityId = frame?.entity_id ?? frameId;
    await client.call("frame.deactivate", { id: entityId });
  },

  setFrameSize: async (client, frameId, size) => {
    const frame = get().frames.find((f) => f.id === frameId);
    const entityId = frame?.entity_id ?? frameId;
    await client.call("frame.set_size", { id: entityId, size });
    set((current) => ({
      frames: current.frames.map((f) =>
        f.id === frameId ? { ...f, size } : f,
      ),
    }));
  },

  setFrameMaterial: async (client, frameId, material) => {
    const frame = get().frames.find((f) => f.id === frameId);
    const entityId = frame?.entity_id ?? frameId;
    await client.call("frame.set_material", { id: entityId, material });
    set((current) => ({
      frames: current.frames.map((f) =>
        f.id === frameId ? { ...f, material } : f,
      ),
    }));
  },

  updateFrameMetadata: async (client, frameId, updates) => {
    const frame = get().frames.find((f) => f.id === frameId);
    const entityId = frame?.entity_id ?? frameId;
    const result = await client.call<{ ok: boolean; frame: import("../rpc/types").FrameDTO }>(
      "frame.update",
      { id: entityId, ...updates },
    );
    if (result.frame) {
      set((current) => ({
        frames: current.frames.map((f) =>
          f.id === frameId ? { ...f, ...result.frame } : f,
        ),
      }));
    } else {
      await get().fetchInitialState(client);
    }
  },

  removeComponent: async (client, frameId, componentId) => {
    await client.call("component.remove", { frame_id: frameId, component_id: componentId });
    set((current) => ({
      frames: current.frames.map((frame) =>
        frame.id !== frameId
          ? frame
          : {
              ...frame,
              components: (frame.components ?? []).filter((c) => c.id !== componentId),
              component_count: (frame.component_count ?? 1) - 1,
            },
      ),
    }));
  },

  setComponentSize: async (client, frameId, componentId, size) => {
    await client.call("component.set_size", { frame_id: frameId, component_id: componentId, size });
    set((current) => ({
      frames: current.frames.map((frame) =>
        frame.id !== frameId
          ? frame
          : {
              ...frame,
              components: (frame.components ?? []).map((c) =>
                c.id === componentId ? { ...c, size } : c,
              ),
            },
      ),
    }));
  },

  setComponentMaterial: async (client, frameId, componentId, material) => {
    await client.call("component.set_material", { frame_id: frameId, component_id: componentId, material });
    set((current) => ({
      frames: current.frames.map((frame) =>
        frame.id !== frameId
          ? frame
          : {
              ...frame,
              components: (frame.components ?? []).map((c) =>
                c.id === componentId ? { ...c, material } : c,
              ),
            },
      ),
    }));
  },

  createFromBlueprint: async (client, blueprint, position) => {
    const params: Record<string, unknown> = { blueprint };
    if (position) params.position = position;
    await client.call("frame.create_from_blueprint", params);
    await get().fetchInitialState(client);
  },

  addComponent: async (client, frameId, componentName) => {
    await client.call("component.add", { frame_id: frameId, component_name: componentName });
    await get().fetchInitialState(client);
  },

  setComponentState: async (client, frameId, componentId, state) => {
    const method = state === "active" ? "component.activate" : "component.deactivate";
    const optimisticState = state === "active" ? "ACTIVATING" : "DEACTIVATING";
    try {
      await client.call(method, { frame_id: frameId, component_id: componentId });
      set((current) => ({
        frames: current.frames.map((frame) =>
          frame.id !== frameId
            ? frame
            : {
                ...frame,
                components: (frame.components ?? []).map((component) =>
                  component.id === componentId ? { ...component, state: optimisticState, error: "" } : component,
                ),
              },
        ),
      }));
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      set((current) => ({
        frames: current.frames.map((frame) =>
          frame.id !== frameId
            ? frame
            : {
                ...frame,
                components: (frame.components ?? []).map((component) =>
                  component.id === componentId ? { ...component, error: message } : component,
                ),
              },
        ),
      }));
    }
  },

  repairComponent: async (client, frameId, componentId) => {
    try {
      await client.call("component.repair", { frame_id: frameId, component_id: componentId });
    } catch {
      return;
    }
    set((current) => ({
      frames: current.frames.map((frame) =>
        frame.id !== frameId
          ? frame
          : {
              ...frame,
              components: (frame.components ?? []).map((component) =>
                component.id === componentId
                  ? { ...component, state: "DEACTIVATED", error: "" }
                  : component,
              ),
            },
      ),
    }));
  },

  updateComponentAttribute: async (client, frameId, componentId, key, value) => {
    try {
      await client.call("component.set_attribute", {
        frame_id: frameId,
        component_id: componentId,
        key,
        value,
      });
    } catch {
      // Keep local fallback for parity slices until backend capability is available.
    }
    set((current) => ({
      frames: current.frames.map((frame) =>
        frame.id !== frameId
          ? frame
          : {
              ...frame,
              components: (frame.components ?? []).map((component) =>
                component.id === componentId
                  ? {
                      ...component,
                      attributes: {
                        ...(component.attributes ?? {}),
                        [key]: value,
                      },
                    }
                  : component,
              ),
            },
      ),
    }));
  },

  storageAddSlot: async (client, frameId, componentId) => {
    await client.call("storage.add_slot", { frame_id: frameId, component_id: componentId });
    await get().fetchInitialState(client);
  },

  storageSetSlot: async (client, frameId, componentId, slotId, itemId, amount) => {
    await client.call("storage.set_slot", {
      frame_id: frameId,
      component_id: componentId,
      slot_id: slotId,
      item_id: itemId,
      amount,
    });
    await get().fetchInitialState(client);
  },

  storageSetAmount: async (client, frameId, componentId, slotId, amount) => {
    await client.call("storage.set_amount", {
      frame_id: frameId,
      component_id: componentId,
      slot_id: slotId,
      amount,
    });
    await get().fetchInitialState(client);
  },

  storageClearSlot: async (client, frameId, componentId, slotId) => {
    await client.call("storage.clear_slot", {
      frame_id: frameId,
      component_id: componentId,
      slot_id: slotId,
    });
    await get().fetchInitialState(client);
  },

  loadCoreCode: async (client, frameId) => {
    const result = await client.call<{ script: string }>("code.get_script", { frame_id: frameId });
    return result.script ?? "";
  },

  executeCoreUpdate: async (client, frameId) => {
    const result = await client.call<{ status: "ok" | "error"; error?: string }>("code.execute", {
      frame_id: frameId,
      function: "update",
    });
    return result;
  },

  createConnection: async (client, source, target, type, medium) => {
    await client.call("connection.create", { source, target, type, medium });
    await get().fetchInitialState(client);
  },

  removeConnection: async (client, id) => {
    await client.call("connection.remove", { id });
    await get().fetchInitialState(client);
  },

  updateCoreCode: async (client, frameId, code) => {
    await client.call("code.update", { frame_id: frameId, code });
  },

  pauseGame: async (client) => {
    await client.call("game.pause");
    set({ timeControl: { ...get().timeControl, paused: true } });
  },

  resumeGame: async (client) => {
    await client.call("game.resume");
    set({ timeControl: { ...get().timeControl, paused: false } });
  },

  setSpeed: async (client, multiplier) => {
    const result = await client.call<{ multiplier: number }>("game.speed.set", { multiplier });
    set({ timeControl: { ...get().timeControl, multiplier: result.multiplier } });
  },

  refreshEnvStatus: async (client) => {
    try {
      const result = await client.call<{ env: EnvironmentDTO }>("env.status");
      set({
        environment: result.env,
        environmentHistory: result.env.history ?? null,
      });
    } catch {
      // env.status may not be available yet
    }
  },

  refreshPowerNetworks: async (client) => {
    try {
      const result = await client.call<{ networks: PowerNetworkDTO[] }>("power.networks");
      set({
        powerNetworks: result.networks ?? [],
      });
    } catch {
      // power.networks may not be available yet
    }
  },

  fetchSpeedState: async (client) => {
    try {
      const result = await client.call<{ paused: boolean; multiplier: number }>("game.speed.get");
      set({ timeControl: { paused: result.paused, multiplier: result.multiplier } });
    } catch {
      // game.speed.get may not be available yet
    }
  },

  removeFrame: async (client, frameId) => {
    const frame = get().frames.find((f) => f.id === frameId);
    const entityId = frame?.entity_id ?? frameId;
    await client.call("frame.remove", { id: entityId });
    set((current) => {
      const nextSel = current.selectedFrameIds.filter((id) => id !== frameId);
      let primary = current.selectedFrameId;
      if (primary === frameId) {
        primary = nextSel.length ? nextSel[nextSel.length - 1]! : null;
      }
      return {
        frames: current.frames.filter((f) => f.id !== frameId),
        selectedFrameIds: nextSel,
        selectedFrameId: primary,
      };
    });
  },

  setEnvironmentField: async (client, field, value) => {
    await client.call("env.set_field", { field, value });
  },
}));
