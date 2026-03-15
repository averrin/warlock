import { useMemo } from "react";
import { RpcClient } from "../rpc/client";

export function useRpcClient() {
  return useMemo(() => {
    const host = window.location.hostname || "127.0.0.1";
    return new RpcClient(`ws://${host}:9800`);
  }, []);
}
