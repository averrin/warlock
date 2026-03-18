import { useEffect, useState } from "react";
import type { RpcClient } from "../../rpc/client";
import type { FrameDTO } from "../../rpc/types";
import { useGameStore } from "../../stores/game";
import { capabilityMethods, isFeatureSupported } from "../../capabilities";
import { CollapsibleSection, SelectField, NumberField, FRAME_SIZES, MATERIALS } from "../ui";
import { FrameHeader } from "./FrameHeader";
import { FrameControls } from "./FrameControls";
import { ComponentStrip } from "./ComponentStrip";
import { ComponentCard } from "./ComponentCard";
import { StorageBar } from "./StorageBar";
import { ErrorList } from "./ErrorList";
import { ComponentContextMenu } from "../canvas/ComponentContextMenu";

type Props = {
  frame: FrameDTO;
  rpcClient: RpcClient;
  level?: 0 | 1 | 2;
  defaultLevel?: 0 | 1 | 2;
  onLevelChange?: (level: 0 | 1 | 2) => void;
  showExpandButton?: boolean;
  onNavigateToTree?: () => void;
};

export function FramePanel({
  frame,
  rpcClient,
  level: controlledLevel,
  defaultLevel = 0,
  onLevelChange,
  showExpandButton = true,
  onNavigateToTree,
}: Props) {
  const [internalLevel, setInternalLevel] = useState(defaultLevel);
  const level = controlledLevel ?? internalLevel;

  const powerNetworks = useGameStore((s) => s.powerNetworks);
  const updateFrameMetadata = useGameStore((s) => s.updateFrameMetadata);
  const moveFrame = useGameStore((s) => s.moveFrame);
  const setFrameSize = useGameStore((s) => s.setFrameSize);
  const setFrameMaterial = useGameStore((s) => s.setFrameMaterial);

  const componentAddSupported = isFeatureSupported(capabilityMethods.componentPalette);
  const [availableComponents, setAvailableComponents] = useState<string[]>([]);
  const [compContextMenu, setCompContextMenu] = useState<{ x: number; y: number; componentId: number } | null>(null);
  const [activeTab, setActiveTab] = useState<"components" | "data">("components");

  const powerNetwork = powerNetworks.find((net) => net.frames.includes(frame.id)) ?? null;
  const components = frame.components ?? [];

  useEffect(() => {
    if (!componentAddSupported || level < 1) return;
    void rpcClient
      .call<{ sources: Record<string, string> }>("code.sources")
      .then((data) => setAvailableComponents(Object.keys(data.sources ?? {})))
      .catch(() => setAvailableComponents([]));
  }, [rpcClient, componentAddSupported, level]);

  const setLevel = (newLevel: 0 | 1 | 2) => {
    setInternalLevel(newLevel);
    onLevelChange?.(newLevel);
  };

  const handleComponentContextMenu = (e: React.MouseEvent, componentId: number) => {
    setCompContextMenu({ x: e.clientX, y: e.clientY, componentId });
  };

  // Level 0: Compact tree row
  if (level === 0) {
    return (
      <div
        style={{
          display: "flex",
          alignItems: "center",
          gap: 8,
          padding: "4px 8px",
          borderRadius: 4,
          background: "#0f172a",
          cursor: "pointer",
        }}
        onClick={() => setLevel(1)}
      >
        <FrameHeader frame={frame} powerNetwork={powerNetwork} compact />
        <div style={{ flex: 1 }} />
        <ComponentStrip frameId={frame.id} components={components} rpcClient={rpcClient} />
        {showExpandButton && <span style={{ color: "#6b7280", fontSize: 12 }}>▸</span>}
      </div>
    );
  }

  // Level 1: Mini inspector
  if (level === 1) {
    return (
      <div style={{ border: "1px solid #1f2937", borderRadius: 8, background: "#111827", padding: 10 }}>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", gap: 8, marginBottom: 8 }}>
          <FrameHeader
            frame={frame}
            powerNetwork={powerNetwork}
            onRename={(name) => void updateFrameMetadata(rpcClient, frame.id, { name })}
          />
          <FrameControls
            frameId={frame.id}
            rpcClient={rpcClient}
            showExpand={showExpandButton}
            onExpand={() => setLevel(2)}
          />
        </div>

        <StorageBar frameId={frame.id} components={components} rpcClient={rpcClient} />

        <div style={{ borderTop: "1px solid #1e293b", paddingTop: 6, marginTop: 6 }}>
          <ComponentStrip
            frameId={frame.id}
            components={components}
            rpcClient={rpcClient}
            availableComponents={level >= 1 ? availableComponents : undefined}
            onComponentContextMenu={handleComponentContextMenu}
          />
        </div>

        <ErrorList frame={frame} />

        <div style={{ marginTop: 8, display: "flex", gap: 4 }}>
          <button type="button" onClick={() => setLevel(0)} style={{ fontSize: 10, padding: "2px 6px" }}>
            Collapse
          </button>
          {onNavigateToTree && (
            <button type="button" onClick={onNavigateToTree} style={{ fontSize: 10, padding: "2px 6px" }}>
              Open in Tree
            </button>
          )}
        </div>

        {compContextMenu && (() => {
          const comp = components.find((c) => c.id === compContextMenu.componentId);
          if (!comp) return null;
          return (
            <ComponentContextMenu
              x={compContextMenu.x}
              y={compContextMenu.y}
              frameId={frame.id}
              componentId={compContextMenu.componentId}
              component={comp}
              onClose={() => setCompContextMenu(null)}
              rpcClient={rpcClient}
            />
          );
        })()}
      </div>
    );
  }

  // Level 2: Full editor
  return (
    <div style={{ border: "1px solid #1f2937", borderRadius: 8, background: "#111827", padding: 10 }}>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", gap: 8, marginBottom: 8 }}>
        <FrameHeader
          frame={frame}
          powerNetwork={powerNetwork}
          onRename={(name) => void updateFrameMetadata(rpcClient, frame.id, { name })}
        />
        <FrameControls frameId={frame.id} rpcClient={rpcClient} showRefresh />
      </div>

      {/* Tabs */}
      <div style={{ display: "flex", gap: 0, borderBottom: "1px solid #334155", marginBottom: 8 }}>
        {(["components", "data"] as const).map((tab) => (
          <button
            key={tab}
            onClick={() => setActiveTab(tab)}
            style={{
              padding: "4px 12px",
              fontSize: 12,
              fontWeight: activeTab === tab ? 600 : 400,
              borderBottom: activeTab === tab ? "2px solid #60a5fa" : "2px solid transparent",
              background: "transparent",
              color: activeTab === tab ? "#e2e8f0" : "#9ca3af",
              cursor: "pointer",
              border: "none",
            }}
          >
            {tab.charAt(0).toUpperCase() + tab.slice(1)}
          </button>
        ))}
      </div>

      {activeTab === "components" ? (
        <div style={{ display: "grid", gap: 8 }}>
          <ComponentStrip
            frameId={frame.id}
            components={components}
            rpcClient={rpcClient}
            availableComponents={availableComponents}
            onComponentContextMenu={handleComponentContextMenu}
          />
          {components.map((comp) => (
            <ComponentCard key={comp.id} component={comp} frameId={frame.id} rpcClient={rpcClient} />
          ))}
        </div>
      ) : (
        <div style={{ display: "grid", gap: 8 }}>
          <div style={{ display: "flex", gap: 12, flexWrap: "wrap" }}>
            <SelectField
              label="Size"
              value={frame.size}
              options={FRAME_SIZES}
              onChange={(v) => void setFrameSize(rpcClient, frame.id, v)}
              labelWidth={50}
            />
            <SelectField
              label="Material"
              value={frame.material ?? "STEEL"}
              options={MATERIALS}
              onChange={(v) => void setFrameMaterial(rpcClient, frame.id, v)}
              labelWidth={60}
            />
          </div>
          {frame.position && (
            <div style={{ display: "flex", gap: 8 }}>
              <NumberField
                label="X"
                value={frame.position.x}
                onChange={(v) => void moveFrame(rpcClient, frame.id, v, frame.position?.y ?? 0)}
                labelWidth={20}
              />
              <NumberField
                label="Y"
                value={frame.position.y}
                onChange={(v) => void moveFrame(rpcClient, frame.id, frame.position?.x ?? 0, v)}
                labelWidth={20}
              />
            </div>
          )}
        </div>
      )}

      <div style={{ marginTop: 8 }}>
        <button type="button" onClick={() => setLevel(1)} style={{ fontSize: 10, padding: "2px 6px" }}>
          Collapse
        </button>
      </div>

      {compContextMenu && (() => {
        const comp = components.find((c) => c.id === compContextMenu.componentId);
        if (!comp) return null;
        return (
          <ComponentContextMenu
            x={compContextMenu.x}
            y={compContextMenu.y}
            frameId={frame.id}
            componentId={compContextMenu.componentId}
            component={comp}
            onClose={() => setCompContextMenu(null)}
            rpcClient={rpcClient}
          />
        );
      })()}
    </div>
  );
}
