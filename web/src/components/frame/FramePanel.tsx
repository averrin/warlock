import { useEffect, useState, type CSSProperties } from "react";
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

// Two fixed slots: [LEFT=collapse][RIGHT=expand or collapse-at-max]
// Level 0: [ ][▸]   click ▸ → 1
// Level 1: [◂][▸]   click ▸ → 2 (same spot), click ◂ → 0
// Level 2: [ ][◂]   click ◂ → 1 (under cursor from previous ▸ click)
function LevelControls({
  level,
  onSetLevel,
}: {
  level: 0 | 1 | 2;
  onSetLevel: (l: 0 | 1 | 2) => void;
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
  const placeholder: CSSProperties = { width: 20, height: 20, flexShrink: 0 };

  return (
    <div style={{ display: "flex", gap: 2, alignItems: "center", flexShrink: 0 }}>
      {/* LEFT slot: collapse (only at level 1) */}
      {level === 1 ? (
        <button
          type="button"
          style={btn}
          title="Collapse"
          onClick={(e) => { e.stopPropagation(); onSetLevel(0); }}
        >
          ◂
        </button>
      ) : (
        <div style={placeholder} />
      )}
      {/* RIGHT slot: expand (levels 0–1) or collapse (level 2) */}
      {level < 2 ? (
        <button
          type="button"
          style={btn}
          title={level === 0 ? "Expand" : "Expand more"}
          onClick={(e) => { e.stopPropagation(); onSetLevel((level + 1) as 1 | 2); }}
        >
          ▸
        </button>
      ) : (
        <button
          type="button"
          style={btn}
          title="Collapse"
          onClick={(e) => { e.stopPropagation(); onSetLevel(1); }}
        >
          ◂
        </button>
      )}
    </div>
  );
}

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
        {showExpandButton && <LevelControls level={0} onSetLevel={setLevel} />}
      </div>
    );
  }

  // Level 1: Mini inspector
  if (level === 1) {
    return (
      <div style={{ border: "1px solid #1f2937", borderRadius: 8, background: "#111827", padding: 10 }}>
        <div
          style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", gap: 8, marginBottom: 8, cursor: "pointer" }}
          onClick={() => setLevel(0)}
        >
          <div onClick={(e) => e.stopPropagation()}>
            <FrameHeader
              frame={frame}
              powerNetwork={powerNetwork}
              onRename={(name) => void updateFrameMetadata(rpcClient, frame.id, { name })}
            />
          </div>
          <div style={{ display: "flex", gap: 6, alignItems: "center", flexShrink: 0 }} onClick={(e) => e.stopPropagation()}>
            <FrameControls frameId={frame.id} rpcClient={rpcClient} />
            {showExpandButton && <LevelControls level={1} onSetLevel={setLevel} />}
          </div>
        </div>

        <StorageBar frameId={frame.id} components={components} rpcClient={rpcClient} />

        <div style={{ borderTop: "1px solid #1e293b", paddingTop: 6, marginTop: 6 }}>
          <ComponentStrip
            frameId={frame.id}
            components={components}
            rpcClient={rpcClient}
            availableComponents={availableComponents}
            onComponentContextMenu={handleComponentContextMenu}
          />
        </div>

        <ErrorList frame={frame} />

        {onNavigateToTree && (
          <div style={{ marginTop: 6 }}>
            <button
              type="button"
              onClick={onNavigateToTree}
              style={{ fontSize: 10, padding: "2px 6px", background: "transparent", border: "1px solid #334155", borderRadius: 3, color: "#9ca3af", cursor: "pointer" }}
            >
              Open in Tree
            </button>
          </div>
        )}

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
      <div
        style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-start", gap: 8, marginBottom: 8, cursor: "pointer" }}
        onClick={() => setLevel(1)}
      >
        <div onClick={(e) => e.stopPropagation()}>
          <FrameHeader
            frame={frame}
            powerNetwork={powerNetwork}
            onRename={(name) => void updateFrameMetadata(rpcClient, frame.id, { name })}
          />
        </div>
        <div style={{ display: "flex", gap: 6, alignItems: "center", flexShrink: 0 }} onClick={(e) => e.stopPropagation()}>
          <FrameControls frameId={frame.id} rpcClient={rpcClient} showRefresh />
          {showExpandButton && <LevelControls level={2} onSetLevel={setLevel} />}
        </div>
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
