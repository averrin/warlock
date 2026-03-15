import type { JsonRpcRequest, JsonRpcResponse } from "./types";

type Pending = {
  resolve: (value: any) => void;
  reject: (reason?: unknown) => void;
  timeout: ReturnType<typeof setTimeout>;
};

type Listener = (payload: unknown) => void;
type LifecycleListener = () => void;

export class RpcClient {
  private ws: WebSocket | null = null;
  private url: string;
  private nextId = 1;
  private pending = new Map<number, Pending>();
  private listeners = new Map<string, Set<Listener>>();
  private lifecycle = {
    connected: new Set<LifecycleListener>(),
    disconnected: new Set<LifecycleListener>(),
    error: new Set<LifecycleListener>(),
  };
  private reconnectAttempts = 0;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private shouldReconnect = true;

  constructor(url: string) {
    this.url = url;
  }

  connect(): void {
    if (
      this.ws &&
      (this.ws.readyState === WebSocket.OPEN ||
        this.ws.readyState === WebSocket.CONNECTING)
    ) {
      return;
    }

    this.ws = new WebSocket(this.url);
    this.ws.onopen = () => {
      this.reconnectAttempts = 0;
      this.emitLifecycle("connected");
    };

    this.ws.onclose = () => {
      this.emitLifecycle("disconnected");
      if (this.shouldReconnect) {
        this.scheduleReconnect();
      }
    };

    this.ws.onerror = () => {
      this.emitLifecycle("error");
    };

    this.ws.onmessage = (event) => {
      this.handleMessage(event.data);
    };
  }

  close(): void {
    this.shouldReconnect = false;
    if (this.reconnectTimer !== null) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    for (const [id, pending] of this.pending.entries()) {
      clearTimeout(pending.timeout);
      // Close can happen during teardown; reject in a microtask so
      // app-level unmount handlers can attach error boundaries first.
      queueMicrotask(() => pending.reject(new Error(`Request cancelled: ${id}`)));
    }
    this.pending.clear();
    this.ws?.close();
    this.ws = null;
  }

  call<T = unknown>(method: string, params: Record<string, unknown> = {}): Promise<T> {
    const id = this.nextId++;
    const req: JsonRpcRequest = {
      jsonrpc: "2.0",
      id,
      method,
      params,
    };
    return new Promise<T>((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`RPC timeout: ${method}`));
      }, 10_000);
      this.pending.set(id, { resolve, reject, timeout });
      void this.ensureOpen()
        .then(() => {
          this.send(req);
        })
        .catch((error) => {
          clearTimeout(timeout);
          this.pending.delete(id);
          reject(error);
        });
    });
  }

  notify(method: string, params: Record<string, unknown> = {}): void {
    const req: JsonRpcRequest = {
      jsonrpc: "2.0",
      method,
      params,
    };
    this.send(req);
  }

  on(method: string, callback: Listener): () => void {
    if (!this.listeners.has(method)) {
      this.listeners.set(method, new Set());
    }
    this.listeners.get(method)!.add(callback);
    return () => {
      this.listeners.get(method)?.delete(callback);
    };
  }

  onLifecycle(kind: "connected" | "disconnected" | "error", callback: LifecycleListener): () => void {
    this.lifecycle[kind].add(callback);
    return () => {
      this.lifecycle[kind].delete(callback);
    };
  }

  private send(req: JsonRpcRequest): void {
    const raw = JSON.stringify(req);
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      throw new Error("WebSocket is not connected");
    }
    this.ws.send(raw);
  }

  private ensureOpen(): Promise<void> {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      return Promise.resolve();
    }
    if (!this.ws || this.ws.readyState === WebSocket.CLOSED) {
      this.connect();
    }
    return new Promise<void>((resolve, reject) => {
      const onConnected = this.onLifecycle("connected", () => {
        clearTimeout(timer);
        onConnected();
        onError();
        resolve();
      });
      const onError = this.onLifecycle("error", () => {
        clearTimeout(timer);
        onConnected();
        onError();
        reject(new Error("WebSocket connect failed"));
      });
      const timer = setTimeout(() => {
        onConnected();
        onError();
        reject(new Error("WebSocket connect timeout"));
      }, 3000);
    });
  }

  private handleMessage(raw: string): void {
    const msg = JSON.parse(raw) as JsonRpcResponse;

    if (typeof msg.id === "number") {
      const pending = this.pending.get(msg.id);
      if (!pending) {
        return;
      }
      clearTimeout(pending.timeout);
      this.pending.delete(msg.id);

      if (msg.error) {
        pending.reject(new Error(`${msg.error.code}: ${msg.error.message}`));
      } else {
        pending.resolve(msg.result);
      }
      return;
    }

    if (msg.method) {
      const callbacks = this.listeners.get(msg.method);
      if (!callbacks) {
        return;
      }
      for (const cb of callbacks) {
        cb(msg.params);
      }
    }
  }

  private scheduleReconnect(): void {
    if (this.reconnectTimer !== null) {
      return;
    }
    const delay = Math.min(5000, 200 * 2 ** this.reconnectAttempts);
    this.reconnectAttempts += 1;
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this.connect();
    }, delay);
  }

  private emitLifecycle(kind: "connected" | "disconnected" | "error"): void {
    for (const callback of this.lifecycle[kind]) {
      callback();
    }
  }
}
