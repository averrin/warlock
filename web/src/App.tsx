import { useEffect } from "react";
import { useRpcClient } from "./hooks/useRpc";
import { useConnectionStore } from "./stores/connection";
import { useGameStore } from "./stores/game";
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
      void fetchInitialState(client).catch(() => {});
      void refreshScene(client).catch(() => {});
    });
    client.connect();
    return () => {
      offConnected();
      client.close();
    };
  }, [client, fetchInitialState, initConnection, initGame, initScene, refreshScene]);

  return <AppShell rpcClient={client} />;
}
