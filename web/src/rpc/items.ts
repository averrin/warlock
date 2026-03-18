import type { RpcClient } from "./client";
import type { ItemsListResultDTO } from "./types";

export async function listItems(client: RpcClient): Promise<ItemsListResultDTO> {
  return await client.call<ItemsListResultDTO>("items.list");
}

