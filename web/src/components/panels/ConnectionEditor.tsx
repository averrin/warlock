import { useState } from "react";
import type { RpcClient } from "../../rpc/client";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { useGameStore } from "../../stores/game";
import { Panel } from "./Panel";

type Props = {
  rpcClient: RpcClient;
};

export function ConnectionEditor({ rpcClient }: Props) {
  const frames = useGameStore((s) => s.frames);
  const connections = useGameStore((s) => s.connections);
  const selectedFrameId = useGameStore((s) => s.selectedFrameId);
  const createConnection = useGameStore((s) => s.createConnection);
  const removeConnection = useGameStore((s) => s.removeConnection);
  const supported = isFeatureSupported(capabilityMethods.connectionEditor);
  const [source, setSource] = useState<number | null>(null);
  const [target, setTarget] = useState<number | null>(null);
  const [type, setType] = useState<"POWER" | "DATA" | "CONVEYOR" | "POE">("POWER");
  const [medium, setMedium] = useState<"WIRE" | "WIRELESS" | "BEAM">("WIRE");

  const sourceValue = source ?? selectedFrameId ?? null;
  const targetCandidates = frames.filter((frame) => frame.id !== sourceValue);

  return (
    <Panel title="Connection Editor" unsupported={supported ? undefined : "connection editor"}>
      <div style={{ display: "grid", gap: 8, fontSize: 12 }}>
        <label>
          Connection Source
          <select
            aria-label="Connection Source"
            value={sourceValue ?? ""}
            onChange={(e) => setSource(Number(e.target.value) || null)}
          >
            <option value="">Select source</option>
            {frames.map((frame) => (
              <option key={frame.id} value={frame.id}>
                {frame.name} ({frame.id})
              </option>
            ))}
          </select>
        </label>
        <label>
          Connection Target
          <select
            aria-label="Connection Target"
            value={target ?? ""}
            onChange={(e) => setTarget(Number(e.target.value) || null)}
          >
            <option value="">Select target</option>
            {targetCandidates.map((frame) => (
              <option key={frame.id} value={frame.id}>
                {frame.name} ({frame.id})
              </option>
            ))}
          </select>
        </label>
        <label>
          Connection Type
          <select
            aria-label="Connection Type"
            value={type}
            onChange={(e) => setType(e.target.value as "POWER" | "DATA" | "CONVEYOR" | "POE")}
          >
            <option value="POWER">POWER</option>
            <option value="DATA">DATA</option>
            <option value="CONVEYOR">CONVEYOR</option>
            <option value="POE">POE</option>
          </select>
        </label>
        <label>
          Connection Medium
          <select aria-label="Connection Medium" value={medium} onChange={(e) => setMedium(e.target.value as any)}>
            <option value="WIRE">WIRE</option>
            <option value="WIRELESS">WIRELESS</option>
            <option value="BEAM">BEAM</option>
          </select>
        </label>
        <button
          disabled={!supported || sourceValue === null || target === null}
          onClick={() => {
            if (sourceValue === null || target === null) {
              return;
            }
            createConnection(rpcClient, sourceValue, target, type, medium).catch(console.error);
          }}
        >
          Create Connection
        </button>
        <div>Connections: {connections.length}</div>
        {connections.map((connection) => (
          <div key={connection.id} style={{ display: "flex", justifyContent: "space-between", gap: 8 }}>
            <span>
              {connection.id}: {connection.source} {"->"} {connection.target} ({connection.type})
            </span>
            <button
              disabled={!supported}
              onClick={() => {
                void removeConnection(rpcClient, connection.id);
              }}
            >
              Remove
            </button>
          </div>
        ))}
      </div>
    </Panel>
  );
}
