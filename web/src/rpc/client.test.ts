import { beforeEach, describe, expect, it, vi } from "vitest";
import { RpcClient } from "./client";

class FakeWebSocket {
  static OPEN = 1;
  static CLOSED = 3;
  readyState = FakeWebSocket.OPEN;
  onopen: (() => void) | null = null;
  onclose: (() => void) | null = null;
  onerror: (() => void) | null = null;
  onmessage: ((event: { data: string }) => void) | null = null;
  sent: string[] = [];

  constructor(_url: string) {
    setTimeout(() => this.onopen?.(), 0);
  }

  send(raw: string) {
    this.sent.push(raw);
  }

  close() {
    this.readyState = FakeWebSocket.CLOSED;
    this.onclose?.();
  }
}

describe("RpcClient", () => {
  beforeEach(() => {
    vi.useFakeTimers();
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    (globalThis as any).WebSocket = FakeWebSocket;
  });

  it("resolves call with result response", async () => {
    const client = new RpcClient("ws://localhost:9800");
    client.connect();
    await vi.runAllTimersAsync();

    const callPromise = client.call("session.info");
    const ws = (client as unknown as { ws: FakeWebSocket }).ws;
    await Promise.resolve();
    const sent = JSON.parse(ws.sent[0]) as { id: number };
    ws.onmessage?.({
      data: JSON.stringify({ jsonrpc: "2.0", id: sent.id, result: { ok: true } }),
    });

    await expect(callPromise).resolves.toEqual({ ok: true });
  });
});
