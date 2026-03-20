import { useEffect } from "react";
import { useRpcClient } from "./hooks/useRpc";
import { useConnectionStore } from "./stores/connection";
import { useGameStore } from "./stores/game";
import { usePatchStore, Patch, PatchType } from "./stores/patches";
import { useSceneStore } from "./stores/scene";
import { AppShell } from "./components/layout/AppShell";
import { Toaster, toast, type DefaultToastOptions } from "react-hot-toast";

const warlockToastOptions: DefaultToastOptions = {
  duration: 4500,
  style: {
    background: "#111827",
    color: "#e5e7eb",
    border: "1px solid #374151",
    borderRadius: 6,
    boxShadow: "0 12px 48px rgba(0, 0, 0, 0.55)",
    width: 400,
    maxWidth: "min(400px, calc(100vw - 32px))",
    padding: "12px 16px",
    fontSize: 14,
    lineHeight: 1.45,
  },
  success: {
    duration: 3500,
    style: {
      borderLeft: "3px solid #22c55e",
    },
    iconTheme: {
      primary: "#22c55e",
      secondary: "#111827",
    },
  },
  error: {
    duration: 5500,
    style: {
      borderLeft: "3px solid #ef4444",
    },
    iconTheme: {
      primary: "#ef4444",
      secondary: "#111827",
    },
  },
  blank: {
    style: {
      borderLeft: "3px solid #64748b",
    },
  },
};

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

  useEffect(() => {
    const unsubscribe = client.on("nexus.toast", (payload: any) => {
      const type = payload?.type || "info";
      const message = payload?.message || "";
      if (type === "success") {
        toast.success(message);
      } else if (type === "error") {
        toast.error(message);
      } else {
        toast(message);
      }
    });
    return () => unsubscribe();
  }, [client]);

  return (
    <>
      <Toaster position="bottom-right" toastOptions={warlockToastOptions} />
      <AppShell rpcClient={client} />
    </>
  );
}
