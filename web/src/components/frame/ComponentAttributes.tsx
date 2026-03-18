import type { RpcClient } from "../../rpc/client";
import { useGameStore } from "../../stores/game";
import { NumberField, TextField } from "../ui";

type AttributeValue = string | number | boolean | unknown;

type Props = {
  frameId: number;
  componentId: number;
  attributes: Record<string, AttributeValue> | undefined;
  rpcClient: RpcClient;
};

export function ComponentAttributes({ frameId, componentId, attributes, rpcClient }: Props) {
  const updateComponentAttribute = useGameStore((s) => s.updateComponentAttribute);

  if (!attributes) return null;

  const entries = Object.entries(attributes).filter(([key]) => key !== "code");

  if (entries.length === 0) return null;

  return (
    <div style={{ display: "grid", gap: 4 }}>
      <div style={{ fontSize: 11, fontWeight: 600, color: "#9ca3af" }}>Attributes</div>
      {entries.map(([key, val]) => {
        if (typeof val === "number") {
          return (
            <NumberField
              key={key}
              label={key}
              value={val}
              onChange={(v) => void updateComponentAttribute(rpcClient, frameId, componentId, key, v)}
            />
          );
        }
        if (typeof val === "boolean") {
          return (
            <div key={key} style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
              <span style={{ minWidth: 90, color: "#9ca3af" }}>{key}:</span>
              <input
                type="checkbox"
                checked={val}
                onChange={(e) => void updateComponentAttribute(rpcClient, frameId, componentId, key, e.target.checked)}
              />
            </div>
          );
        }
        if (typeof val === "string") {
          return (
            <TextField
              key={key}
              label={key}
              value={val}
              onChange={(v) => void updateComponentAttribute(rpcClient, frameId, componentId, key, v)}
            />
          );
        }
        return (
          <div key={key} style={{ padding: "1px 0", color: "#6b7280", fontSize: 11 }}>
            {key}: {JSON.stringify(val)}
          </div>
        );
      })}
    </div>
  );
}
