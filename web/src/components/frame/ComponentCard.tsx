import { useState, type CSSProperties } from "react";
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

// Same 2-slot schema as FramePanel LevelControls, but 2-state only.
// Collapsed: [ ][▸]   Expanded: [ ][◂]  — right slot only, ◂ lands under cursor after expanding.
function CardExpandControl({
  expanded,
  onToggle,
}: {
  expanded: boolean;
  onToggle: () => void;
}) {
  const btn: CSSProperties = {
    border: "1px solid #334155",
    borderRadius: 4,
    width: 20,
    height: 20,
    background: "#020617",
    color: "#9ca3af",
    cursor: "pointer",
    fontSize: 11,
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    padding: 0,
    flexShrink: 0,
  };

  return (
    <div style={{ display: "flex", gap: 2, alignItems: "center", flexShrink: 0 }}>
      {/* LEFT slot: always empty placeholder to keep right slot position stable */}
      <div style={{ width: 20, height: 20, flexShrink: 0 }} />
      {/* RIGHT slot: ▸ collapsed → ◂ expanded (same pixel position — ◂ under cursor after expand) */}
      <button
        type="button"
        style={btn}
        title={expanded ? "Collapse" : "Expand"}
        onClick={(e) => { e.stopPropagation(); onToggle(); }}
      >
        {expanded ? "◂" : "▸"}
      </button>
    </div>
  );
}

export function ComponentCard({ component, frameId, rpcClient }: Props) {
  const [expanded, setExpanded] = useState(false);
  const setComponentSize = useGameStore((s) => s.setComponentSize);
  const setComponentMaterial = useGameStore((s) => s.setComponentMaterial);

  const stateColor = STATE_COLORS[component.state] ?? "#1f2937";
  const effects = (component.effects ?? []).map((e) => EFFECT_LABELS[e] ?? e);
  const iconFile = (component.metadata?.icon || component.icon || "").trim();
  const glyph = (component.name || "?")[0]!.toUpperCase();

  const isFrozen = (component.effects ?? []).includes("FREEZE");
  const isOverheat = (component.effects ?? []).includes("OVERHEAT");
  let borderColor = "#334155";
  if (isFrozen) borderColor = "#3b82f6";
  else if (isOverheat) borderColor = "#f97316";

  return (
    <div style={{ border: "1px solid #334155", borderRadius: 6, padding: "6px 8px", display: "grid", gap: 6 }}>
      {/* Header: icon | name id effects badge | flex-1 | action buttons | expand control */}
      <div
        style={{ display: "flex", alignItems: "center", gap: 6, cursor: "pointer" }}
        onClick={() => setExpanded((v) => !v)}
      >
        {/* Component icon badge — same style as ComponentStrip */}
        <div
          style={{
            width: 24,
            height: 24,
            borderRadius: 6,
            border: `1px solid ${borderColor}`,
            background: stateColor,
            display: "flex",
            alignItems: "center",
            justifyContent: "center",
            flexShrink: 0,
          }}
        >
          {iconFile ? (
            <img src={`/icons/${iconFile}`} alt={component.name} style={{ width: 16, height: 16, objectFit: "contain" }} />
          ) : (
            <span style={{ fontSize: 10, fontWeight: 700, color: "#e2e8f0" }}>{glyph}</span>
          )}
        </div>

        <span style={{ fontSize: 12, fontWeight: 600, color: "#e5e7eb" }}>{component.name}</span>
        <span style={{ fontSize: 10, color: "#6b7280" }}>#{component.id}</span>
        {effects.map((eff, i) => (
          <span key={i} style={{ fontSize: 11 }}>{eff}</span>
        ))}
        <Badge label={component.state} variant="state" />

        <div style={{ flex: 1 }} />

        <ComponentControls
          frameId={frameId}
          componentId={component.id}
          componentState={component.state}
          rpcClient={rpcClient}
        />
        <CardExpandControl expanded={expanded} onToggle={() => setExpanded((v) => !v)} />
      </div>

      {/* Error */}
      {component.error && (
        <div style={{ fontSize: 11, color: "#ef4444", padding: "2px 4px", background: "#1c0a0a", borderRadius: 4 }}>
          {component.error}
        </div>
      )}

      {/* Expanded content */}
      {expanded && (
        <>
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
          <ComponentAttributes
            frameId={frameId}
            componentId={component.id}
            attributes={component.attributes as Record<string, unknown> | undefined}
            rpcClient={rpcClient}
          />
          {component.storage && (
            <StoragePanelWithSubscription frameId={frameId} componentId={component.id} mode="full" rpcClient={rpcClient} />
          )}
        </>
      )}
    </div>
  );
}
