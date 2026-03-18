import { useMemo } from "react";
import { RpcClient } from "../rpc/client";

export function useRpcClient() {
  return useMemo(() => {
    const configuredUrl = import.meta.env.VITE_RPC_URL as string | undefined;
    if (configuredUrl) {
      return new RpcClient(configuredUrl);
    }
    const host = window.location.hostname || "127.0.0.1";
    return new RpcClient(`ws://${host}:9800`);
  }, []);
}
