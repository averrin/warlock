import { describe, expect, it, beforeEach, vi } from "vitest";
import { useItemsCatalogStore } from "./itemsCatalog";

function mockRpcClient(responses: Record<string, unknown> = {}) {
  return {
    call: vi.fn(async (method: string) => {
      if (method in responses) return responses[method];
      return {};
    }),
  } as any;
}

describe("items catalog store", () => {
  beforeEach(() => {
    useItemsCatalogStore.getState().reset();
  });

  it("fetchItems populates items and itemsById", async () => {
    const client = mockRpcClient({
      "items.list": {
        items: [
          { id: "Iron", name: "Iron" },
          { id: "Copper", name: "Copper" },
        ],
      },
    });

    await useItemsCatalogStore.getState().fetchItems(client);

    const state = useItemsCatalogStore.getState();
    expect(state.loaded).toBe(true);
    expect(state.items).toHaveLength(2);
    expect(state.itemsById["Iron"]?.name).toBe("Iron");
    expect(state.itemsById["Copper"]?.name).toBe("Copper");
    expect(client.call).toHaveBeenCalledWith("items.list");
  });
});

