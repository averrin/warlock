export type JsonRpcId = number;

export interface JsonRpcRequest {
  jsonrpc: "2.0";
  id?: JsonRpcId;
  method: string;
  params?: Record<string, unknown>;
}

export interface JsonRpcError {
  code: number;
  message: string;
  data?: unknown;
}

export interface JsonRpcResponse {
  jsonrpc: "2.0";
  id?: JsonRpcId;
  result?: unknown;
  error?: JsonRpcError;
  method?: string;
  params?: unknown;
}

export interface SessionInfo {
  claimed_by: string | null;
  clients: number;
  uptime_ms: number;
}

export interface StorageSlotDTO {
  id: number;
  stack: { item: string; item_id?: number; amount: number; max: number } | null;
}

export interface StorageDTO {
  slots_count: number;
  slots: StorageSlotDTO[];
  slots_total?: number;
  slots_used?: number;
  top_items?: { item_id?: number; name: string; amount: number }[];
}

export interface ItemCatalogItemDTO {
  id: string;
  name: string;
}

export interface ItemsListResultDTO {
  items: ItemCatalogItemDTO[];
}

export interface AttributeDTO {
  title: string;
  type: string;
  base_value: string | number | boolean;
  final_value: string | number | boolean;
  modifiers: string[];
}

export interface MetadataDTO {
  id: number;
  name: string;
  description: string;
  icon: string;
  attributes: Record<string, AttributeDTO>;
}

export interface ComponentDTO {
  id: number;
  name: string;
  description?: string;
  icon?: string;
  state: string;
  size?: string;
  material?: string;
  error?: string;
  effects?: string[];
  storage?: StorageDTO | null;
  attributes?: Record<string, unknown>;
  metadata?: MetadataDTO;
  require?: string[];
}

export type CanvasBadgeHealth = "ok" | "warn" | "bad";
export type CanvasBadgePower = "offline" | "deficit" | "balanced" | "surplus";

export interface CanvasBadgesDTO {
  health?: CanvasBadgeHealth;
  power?: CanvasBadgePower;
  temperature?: number;
  has_error?: boolean;
}

export interface FrameDTO {
  entity_id: number;
  id: number;
  name: string;
  size: string;
  material?: string;
  component_count: number;
  components?: ComponentDTO[];
  metadata?: MetadataDTO;
  position?: {
    x: number;
    y: number;
  };
  canvas_badges?: CanvasBadgesDTO;
}

export interface ConnectionDTO {
  id: number;
  source: number;
  target: number;
  type: string;
}

export interface EnvironmentDTO {
  minutes?: number;
  days?: number;
  is_day?: boolean;
  temperature?: number;
  air_flow?: number;
  sun?: number;
  radioactivity?: number;
  history?: {
    temperature?: number[];
    air_flow?: number[];
    sun?: number[];
    [k: string]: number[] | undefined;
  };
  [k: string]: unknown;
}

export interface PowerNetworkDTO {
  name?: string;
  frames: number[];
  production: number;
  consumption: number;
  accumulated: number;
  accumulated_available?: number;
  battery_count?: number;
  history: Record<string, number[]>;
}
