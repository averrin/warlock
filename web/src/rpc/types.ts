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

export interface ComponentDTO {
  id: number;
  name: string;
  state: string;
  attributes?: Record<string, unknown>;
}

export interface FrameDTO {
  entity_id: number;
  id: number;
  name: string;
  size: string;
  component_count: number;
  components?: ComponentDTO[];
  position?: {
    x: number;
    y: number;
  };
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
  [k: string]: unknown;
}

export interface PowerNetworkDTO {
  frames: number[];
  production: number;
  consumption: number;
  accumulated: number;
  history: Record<string, number[]>;
}
