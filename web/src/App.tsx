import { useEffect } from "react";
import { useRpcClient } from "./hooks/useRpc";
import { useConnectionStore } from "./stores/connection";
import { useGameStore } from "./stores/game";
import { usePatchStore, Patch, PatchType } from "./stores/patches";
import { useSceneStore } from "./stores/scene";
import { AppShell } from "./components/layout/AppShell";

export default function App() {
  const client = useRpcClient();
  const initConnection = useConnectionStore((s) => s.init);
  const initGame = useGameStore((s) => s.init);
  const fetchInitialState = useGameStore((s) => s.fetchInitialState);
  const initScene = useSceneStore((s) => s.init);
  const refreshScene = useSceneStore((s) => s.refresh);

  useEffect(() => {
    initConnection(client);
    initGame(client);
    initScene(client);
    const offConnected = client.onLifecycle("connected", () => {
      void (async () => {
        try {
          await client.call("state.subscribe", { topics: [] });
        } catch {
          // Keep startup resilient if subscribe is temporarily unavailable.
        }
        await fetchInitialState(client).catch(() => {});
        await refreshScene(client).catch(() => {});

        // Fetch patches and patch types
        try {
          const patchData = await client.call<{ patches: Patch[] }>("patches.list");
          usePatchStore.getState().setPatches(patchData.patches ?? []);
        } catch (e) {
          console.error("Failed to fetch patches:", e);
        }
        try {
          const typeData = await client.call<{ types: PatchType[] }>("patches.types");
          usePatchStore.getState().setPatchTypes(typeData.types ?? []);
        } catch (e) {
          console.error("Failed to fetch patch types:", e);
        }
      })();
    });
    client.connect();
    return () => {
      offConnected();
      client.close();
    };
  }, [client, fetchInitialState, initConnection, initGame, initScene, refreshScene]);

  return <AppShell rpcClient={client} />;
}
