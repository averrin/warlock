import { describe, expect, it, vi, beforeEach } from "vitest";
import { useGameStore } from "./game";

function mockRpcClient(responses: Record<string, unknown> = {}) {
  const listeners = new Map<string, (payload: unknown) => void>();
  return {
    call: vi.fn(async (method: string) => {
      if (method in responses) return responses[method];
      return {};
    }),
    on: vi.fn((method: string, cb: (payload: unknown) => void) => {
      listeners.set(method, cb);
      return () => listeners.delete(method);
    }),
    onLifecycle: vi.fn(() => () => {}),
    connect: vi.fn(),
    close: vi.fn(),
    notify: vi.fn(),
    emit: (method: string, payload: unknown) => {
      listeners.get(method)?.(payload);
    },
  } as any;
}

describe("game store - speed control", () => {
  beforeEach(() => {
    useGameStore.setState({
      started: false,
      hydrated: false,
      frames: [],
      connections: [],
      powerNetworks: [],
      selectedFrameId: null,
      timeControl: { paused: false, multiplier: 1 },
      environment: null,
      environmentHistory: null,
    });
  });

  it("pauseGame sets paused to true", async () => {
    const client = mockRpcClient();
    await useGameStore.getState().pauseGame(client);
    expect(client.call).toHaveBeenCalledWith("game.pause");
    expect(useGameStore.getState().timeControl.paused).toBe(true);
  });

  it("resumeGame sets paused to false", async () => {
    useGameStore.setState({ timeControl: { paused: true, multiplier: 1 } });
    const client = mockRpcClient();
    await useGameStore.getState().resumeGame(client);
    expect(client.call).toHaveBeenCalledWith("game.resume");
    expect(useGameStore.getState().timeControl.paused).toBe(false);
  });

  it("setSpeed updates multiplier from server response", async () => {
    const client = mockRpcClient({ "game.speed.set": { multiplier: 5 } });
    await useGameStore.getState().setSpeed(client, 5);
    expect(client.call).toHaveBeenCalledWith("game.speed.set", { multiplier: 5 });
    expect(useGameStore.getState().timeControl.multiplier).toBe(5);
  });

  it("fetchSpeedState populates timeControl from server", async () => {
    const client = mockRpcClient({ "game.speed.get": { paused: true, multiplier: 10 } });
    await useGameStore.getState().fetchSpeedState(client);
    expect(useGameStore.getState().timeControl).toEqual({ paused: true, multiplier: 10 });
  });

  it("fetchSpeedState silently handles errors", async () => {
    const client = mockRpcClient();
    client.call = vi.fn(async () => { throw new Error("not available"); });
    await useGameStore.getState().fetchSpeedState(client);
    // Should not throw, state unchanged
    expect(useGameStore.getState().timeControl).toEqual({ paused: false, multiplier: 1 });
  });
});

describe("game store - environment status", () => {
  beforeEach(() => {
    useGameStore.setState({
      started: false,
      hydrated: false,
      frames: [],
      connections: [],
      powerNetworks: [],
      selectedFrameId: null,
      environment: null,
      environmentHistory: null,
    });
  });

  it("refreshEnvStatus populates environment and history", async () => {
    const envData = {
      minutes: 720,
      days: 3,
      is_day: true,
      temperature: 45.5,
      air_flow: 12.0,
      sun: 80.0,
      history: {
        temperature: [40, 42, 45],
        air_flow: [10, 11, 12],
        sun: [70, 75, 80],
      },
    };
    const client = mockRpcClient({ "env.status": { env: envData } });
    await useGameStore.getState().refreshEnvStatus(client);

    const state = useGameStore.getState();
    expect(state.environment?.temperature).toBe(45.5);
    expect(state.environment?.days).toBe(3);
    expect(state.environmentHistory?.temperature).toEqual([40, 42, 45]);
    expect(state.environmentHistory?.sun).toEqual([70, 75, 80]);
  });

  it("refreshEnvStatus silently handles errors", async () => {
    const client = mockRpcClient();
    client.call = vi.fn(async () => { throw new Error("no env"); });
    await useGameStore.getState().refreshEnvStatus(client);
    expect(useGameStore.getState().environment).toBeNull();
  });
});

describe("game store - snapshot and push updates", () => {
  beforeEach(() => {
    useGameStore.setState({
      started: false,
      hydrated: false,
      frames: [],
      connections: [],
      powerNetworks: [],
      selectedFrameId: null,
      environment: null,
      environmentHistory: null,
      timeControl: { paused: false, multiplier: 1 },
    });
  });

  it("fetchInitialState hydrates from state.snapshot", async () => {
    const snapshot = {
      started: true,
      frames: [{ id: 1, entity_id: 1, name: "Frame", size: "M", component_count: 0 }],
      connections: [],
      environment: { temperature: 21, history: { temperature: [21] } },
      power_networks: [{ frames: [1], production: 1, consumption: 1, accumulated: 0, history: {} }],
    };
    const client = mockRpcClient({ "state.snapshot": snapshot });

    await useGameStore.getState().fetchInitialState(client);

    const state = useGameStore.getState();
    expect(state.hydrated).toBe(true);
    expect(state.started).toBe(true);
    expect(state.frames).toHaveLength(1);
    expect(state.environment?.temperature).toBe(21);
    expect(state.powerNetworks).toHaveLength(1);
  });

  it("applies event.state_update and env/power push events", () => {
    const client = mockRpcClient();
    useGameStore.getState().init(client);

    client.emit("event.state_update", {
      frames: [{ id: 9, entity_id: 9, name: "Realtime", size: "L", component_count: 0 }],
      connections: [{ id: 3, source: 1, target: 2, type: "POWER" }],
    });
    client.emit("event.env_update", { env: { temperature: 42, history: { temperature: [40, 42] } } });
    client.emit("event.power_update", { networks: [{ frames: [9], production: 5, consumption: 4, accumulated: 1, history: {} }] });

    const state = useGameStore.getState();
    expect(state.frames[0]?.id).toBe(9);
    expect(state.connections[0]?.id).toBe(3);
    expect(state.environment?.temperature).toBe(42);
    expect(state.environmentHistory?.temperature).toEqual([40, 42]);
    expect(state.powerNetworks[0]?.production).toBe(5);
  });

  it("preserves canvas_badges on frames from snapshot", async () => {
    const snapshot = {
      started: true,
      frames: [{
        id: 1, entity_id: 1, name: "F", size: "M", component_count: 2,
        canvas_badges: {
          health: "warn",
          power: "deficit",
          temperature: 78.5,
          has_error: true,
        },
      }],
      connections: [],
      environment: null,
      power_networks: [],
    };
    const client = mockRpcClient({ "state.snapshot": snapshot });
    await useGameStore.getState().fetchInitialState(client);

    const frame = useGameStore.getState().frames[0];
    expect(frame?.canvas_badges).toBeDefined();
    expect(frame?.canvas_badges?.health).toBe("warn");
    expect(frame?.canvas_badges?.power).toBe("deficit");
    expect(frame?.canvas_badges?.temperature).toBe(78.5);
    expect(frame?.canvas_badges?.has_error).toBe(true);
  });

  it("preserves canvas_badges from state_update events", () => {
    const client = mockRpcClient();
    useGameStore.getState().init(client);

    client.emit("event.state_update", {
      frames: [{
        id: 5, entity_id: 5, name: "Hot", size: "S", component_count: 0,
        canvas_badges: { health: "ok", power: "balanced", has_error: false },
      }],
      connections: [],
    });

    const frame = useGameStore.getState().frames[0];
    expect(frame?.canvas_badges?.health).toBe("ok");
    expect(frame?.canvas_badges?.power).toBe("balanced");
    expect(frame?.canvas_badges?.has_error).toBe(false);
  });

  it("preserves storage summary fields on components from snapshot", async () => {
    const snapshot = {
      started: true,
      frames: [{
        id: 1,
        entity_id: 1,
        name: "StorageFrame",
        size: "M",
        component_count: 1,
        components: [{
          id: 10,
          name: "StorageComponent",
          state: "active",
          storage: {
            slots_count: 3,
            slots_total: 3,
            slots_used: 2,
            top_items: [
              { item_id: 123, name: "Iron", amount: 9 },
              { name: "Copper", amount: 2 },
            ],
            slots: [
              { id: 0, stack: { item: "Iron", item_id: 123, amount: 9, max: 99 } },
              { id: 1, stack: { item: "Copper", amount: 2, max: 99 } },
              { id: 2, stack: null },
            ],
          },
        }],
      }],
      connections: [],
      environment: null,
      power_networks: [],
    };

    const client = mockRpcClient({ "state.snapshot": snapshot });
    await useGameStore.getState().fetchInitialState(client);

    const storage = useGameStore.getState().frames[0]?.components?.[0]?.storage;
    expect(storage?.slots_count).toBe(3);
    expect(storage?.slots_total).toBe(3);
    expect(storage?.slots_used).toBe(2);
    expect(storage?.top_items).toEqual([
      { item_id: 123, name: "Iron", amount: 9 },
      { name: "Copper", amount: 2 },
    ]);
    expect(storage?.slots).toEqual([
      { id: 0, stack: { item: "Iron", item_id: 123, amount: 9, max: 99 } },
      { id: 1, stack: { item: "Copper", amount: 2, max: 99 } },
      { id: 2, stack: null },
    ]);
  });
});
