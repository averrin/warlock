/**
 * @vitest-environment jsdom
 */
import { describe, expect, it, vi, beforeEach } from "vitest";
import { createRoot } from "react-dom/client";
import { flushSync } from "react-dom";
import { createElement } from "react";
import { StoragePanel } from "./StoragePanel";
import { RpcClient } from "../../rpc/client";

const mockStorage = {
  slots_count: 3,
  slots_total: 3,
  slots_used: 2,
  top_items: [{ item_id: 1, name: "Iron", amount: 9 }, { name: "Copper", amount: 2 }],
  slots: [
    { id: 0, stack: { item: "Iron", item_id: 1, amount: 9, max: 99 } },
    { id: 1, stack: { item: "Copper", amount: 2, max: 99 } },
    { id: 2, stack: null },
  ],
};

function mockRpc() {
  const client = new RpcClient("ws://test");
  vi.spyOn(client, "call").mockResolvedValue({});
  return client;
}

function render(jsx: ReturnType<typeof createElement>) {
  const container = document.createElement("div");
  document.body.appendChild(container);
  const root = createRoot(container);
  flushSync(() => root.render(jsx));
  return {
    container,
    cleanup: () => {
      root.unmount();
      container.remove();
    },
  };
}

describe("StoragePanel", () => {
  beforeEach(() => {
    vi.resetModules();
  });

  it("compact mode shows used/total and top items", () => {
    const onRefresh = vi.fn(async () => {});
    const { container, cleanup } = render(
      createElement(StoragePanel, {
        storage: mockStorage,
        frameId: 1,
        componentId: 10,
        mode: "compact",
        rpcClient: mockRpc(),
        onRefresh,
      })
    );
    const el = container.querySelector('[data-storage-panel="compact"]');
    expect(el).toBeTruthy();
    expect(el?.textContent).toContain("2/3");
    expect(el?.textContent).toContain("Iron");
    expect(el?.textContent).toContain("Copper");
    cleanup();
  });

  it("full mode renders slot list with Add for empty slot", () => {
    const onRefresh = vi.fn(async () => {});
    const { container, cleanup } = render(
      createElement(StoragePanel, {
        storage: mockStorage,
        frameId: 1,
        componentId: 10,
        mode: "full",
        rpcClient: mockRpc(),
        onRefresh,
      })
    );
    const el = container.querySelector('[data-storage-panel="full"]');
    expect(el).toBeTruthy();
    expect(container.querySelector('[data-slot-empty="true"]')).toBeTruthy();
    const addBtns = Array.from(container.querySelectorAll("button")).filter((b) => /add/i.test(b.textContent ?? ""));
    expect(addBtns.length).toBeGreaterThan(0);
    cleanup();
  });
});
