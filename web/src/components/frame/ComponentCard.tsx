import { useState } from "react";
import type { RpcClient } from "../../rpc/client";
import type { ComponentDTO } from "../../rpc/types";
import { Badge, STATE_COLORS, EFFECT_LABELS, SelectField, COMPONENT_SIZES, MATERIALS } from "../ui";
import { useGameStore } from "../../stores/game";
import { ComponentControls } from "./ComponentControls";
import { ComponentAttributes } from "./ComponentAttributes";
import { StoragePanelWithSubscription } from "../storage/StoragePanel";

type Props = {
  component: ComponentDTO;
  frameId: number;
  rpcClient: RpcClient;
};

export function ComponentCard({ component, frameId, rpcClient }: Props) {
  const [expanded, setExpanded] = useState(false);
  const setComponentSize = useGameStore((s) => s.setComponentSize);
  const setComponentMaterial = useGameStore((s) => s.setComponentMaterial);

  const stateColor = STATE_COLORS[component.state] ?? "#1f2937";
  const effects = (component.effects ?? []).map((e) => EFFECT_LABELS[e] ?? e);

  return (
    <div style={{ border: "1px solid #334155", borderRadius: 6, padding: 8, display: "grid", gap: 6 }}>
      {/* Header */}
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", gap: 8 }}>
        <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
          <button
            type="button"
            onClick={() => setExpanded((v) => !v)}
            style={{ border: "none", background: "transparent", color: "#9ca3af", cursor: "pointer", padding: 0, fontSize: 12, width: 14 }}
          >
            {expanded ? "▾" : "▸"}
          </button>
          <span style={{ fontSize: 12, fontWeight: 600 }}>{component.name}</span>
          <span style={{ fontSize: 10, color: "#6b7280" }}>#{component.id}</span>
          {effects.map((eff, i) => (
            <span key={i} style={{ fontSize: 12 }}>{eff}</span>
          ))}
        </div>
        <Badge label={component.state} variant="state" />
      </div>

      {/* Error */}
      {component.error && (
        <div style={{ fontSize: 11, color: "#ef4444", padding: "2px 4px", background: "#1c0a0a", borderRadius: 4 }}>
          {component.error}
        </div>
      )}

      {/* Controls - always visible */}
      <ComponentControls frameId={frameId} componentId={component.id} componentState={component.state} rpcClient={rpcClient} />

      {expanded && (
        <>
          {/* Size/Material */}
          <div style={{ display: "flex", gap: 12, flexWrap: "wrap" }}>
            <SelectField
              label="Size"
              value={component.size ?? "S"}
              options={COMPONENT_SIZES}
              onChange={(v) => void setComponentSize(rpcClient, frameId, component.id, v)}
              labelWidth={50}
            />
            <SelectField
              label="Material"
              value={component.material ?? "STEEL"}
              options={MATERIALS}
              onChange={(v) => void setComponentMaterial(rpcClient, frameId, component.id, v)}
              labelWidth={60}
            />
          </div>

          {/* Attributes */}
          <ComponentAttributes
            frameId={frameId}
            componentId={component.id}
            attributes={component.attributes as Record<string, unknown> | undefined}
            rpcClient={rpcClient}
          />

          {/* Storage */}
          {component.storage && (
            <StoragePanelWithSubscription frameId={frameId} componentId={component.id} mode="full" rpcClient={rpcClient} />
          )}
        </>
      )}
    </div>
  );
}
