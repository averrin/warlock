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

export interface InspectorMeta {
  widget?: "select" | "link" | "progress" | "color" | "code" | "list";
  options?: string[];
  link_scope?: "frame" | "world";
  link_filter?: string;
  min?: number;
  max?: number;
  max_attr?: string;
  min_attr?: string;
  unit?: string;
  readonly?: boolean;
  hidden?: boolean;
  precision?: number;
  label?: string;
  color?: string;
  icon?: string;
}

export interface AttributeDTO {
  title: string;
  type: string;
  base_value: string | number | boolean;
  final_value: string | number | boolean;
  modifiers: string[];
  inspector?: InspectorMeta;
}

export interface MetadataDTO {
  id: number;
  name: string;
  description: string;
  icon: string;
  attributes: Record<string, AttributeDTO>;
}

export interface DataPacketDTO {
  source: number;
  destination: number;
  headers: Record<string, string>;
  body: string;
}

export interface DataLinkBufferDTO {
  counterpart_id: number;
  counterpart_id_alt: number;
  raw_queue: string[];
  packet_queue: DataPacketDTO[];
  max_queue: number;
}

export interface ComponentDTO {
  id: number;
  name: string;
  /** Logical type from the `type` attribute (e.g. Data Connector). */
  type?: string;
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
  data_link_buffer?: DataLinkBufferDTO | null;
}

export type CanvasBadgeHealth = "ok" | "warn" | "bad";
export type CanvasBadgePower = "offline" | "deficit" | "balanced" | "surplus";

export interface CanvasBadgesDTO {
  health?: CanvasBadgeHealth;
  power?: CanvasBadgePower;
  temperature?: number;
  has_error?: boolean;
}

export interface ComponentSlotUsageDTO {
  used: number;
  max: number;
}

export interface FrameDTO {
  entity_id: number;
  id: number;
  name: string;
  size: string;
  material?: string;
  component_count: number;
  components?: ComponentDTO[];
  component_slots?: Record<string, ComponentSlotUsageDTO>;
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
  medium?: string;
  /** 0..1 while an item is moving along this conveyor edge (server-driven). */
  transfer_progress?: number;
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

export interface ControlZoneDTO {
  x: number;
  y: number;
  /** Radius in grid cells. */
  radius: number;
  frame_id: number;
  source: "nexus" | "relay";
}

/** An ECS entity with all its component data as returned by entities.list */
export interface EcsEntityDTO {
  entity_id: number;
  label: string;
  /** ineditor.color — used to tint entity rows in the inspector */
  color?: string;
  components: Record<string, Record<string, unknown> | boolean>;
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

export interface ResearchNodeDTO {
  name: string;
  description: string;
  icon: string;
  cost: Record<string, number>;
  requires: string[];
  unlocks: string[];
  status: "locked" | "available" | "unlocked";
}

export interface ResearchListDTO {
  nodes: ResearchNodeDTO[];
}
