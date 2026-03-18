import { useEffect, useRef } from "react";
import { createPortal } from "react-dom";
import type { RpcClient } from "../../rpc/client";
import type { ComponentDTO } from "../../rpc/types";
import { useGameStore } from "../../stores/game";

const STATE_COLORS: Record<string, string> = {
  ACTIVE: "#14532d",
  ACTIVATING: "#3f6212",
  DEACTIVATING: "#78350f",
  DEACTIVATED: "#1f2937",
  COMP_ERROR: "#7f1d1d",
  DESTROYED: "#450a0a",
  BLOCKED: "#4a1d96",
  BROKEN: "#7f1d1d",
};

type Props = {
  x: number;
  y: number;
  frameId: number;
  componentId: number;
  component: ComponentDTO;
  onClose: () => void;
  rpcClient: RpcClient;
};

export function ComponentContextMenu({ x, y, frameId, componentId, component, onClose, rpcClient }: Props) {
  const setComponentState = useGameStore((s) => s.setComponentState);
  const repairComponent = useGameStore((s) => s.repairComponent);
  const removeComponent = useGameStore((s) => s.removeComponent);
  const menuRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const handleClickOutside = (e: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(e.target as Node)) {
        onClose();
      }
    };
    document.addEventListener("mousedown", handleClickOutside);
    return () => document.removeEventListener("mousedown", handleClickOutside);
  }, [onClose]);

  const state = (component.state ?? "").toUpperCase();
  const stateColor = STATE_COLORS[state] ?? "#1f2937";

  // Extract temperature from attributes
  const metaAttrs = component.metadata?.attributes as
    | Record<string, { final_value?: unknown; base_value?: unknown }>
    | undefined;
  const rawAttrs = component.attributes as Record<string, unknown> | undefined;
  const tempMeta = metaAttrs?.temperature ?? null;
  const tempRaw = (tempMeta?.final_value ?? tempMeta?.base_value) ?? rawAttrs?.temperature;
  const tempValue = typeof tempRaw === "number" ? tempRaw : typeof tempRaw === "string" ? Number(tempRaw) : NaN;

  // Extract power consumption from attributes
  const powerMeta = metaAttrs?.consumption ?? metaAttrs?.activation_consumption ?? null;
  const powerRaw = (powerMeta?.final_value ?? powerMeta?.base_value) ?? rawAttrs?.consumption ?? rawAttrs?.activation_consumption;
  const powerValue = typeof powerRaw === "number" ? powerRaw : typeof powerRaw === "string" ? Number(powerRaw) : NaN;

  const effects = component.effects ?? [];
  const error = component.error?.trim();

  const iconFile = (component.metadata?.icon || component.icon || "").trim();

  // Conditional action visibility
  const showStart = state === "DEACTIVATED";
  const showStop = state === "ACTIVE" || state === "ACTIVATING" || state === "DEACTIVATING";
  const showRepair = state === "COMP_ERROR" || state === "BROKEN";

  const menuRowStyle: React.CSSProperties = {
    padding: "4px 10px",
    cursor: "pointer",
    display: "flex",
    alignItems: "center",
    gap: 8,
  };

  const doAction = (fn: () => void) => {
    fn();
    onClose();
  };

  return createPortal(
    <div
      ref={menuRef}
      data-context-menu
      style={{
        position: "fixed",
        top: y,
        left: x,
        background: "#1e293b",
        border: "1px solid #334155",
        borderRadius: 6,
        padding: "4px 0",
        fontSize: 12,
        color: "#e5e7eb",
        zIndex: 9999,
        minWidth: 190,
        boxShadow: "0 4px 16px rgba(0,0,0,0.5)",
      }}
    >
      {/* Header */}
      <div
        style={{
          padding: "4px 10px 6px",
          fontSize: 11,
          color: "#94a3b8",
          fontWeight: 600,
          borderBottom: "1px solid #1e3a5f",
          display: "flex",
          alignItems: "center",
          gap: 6,
        }}
      >
        {iconFile ? (
          <img src={`/icons/${iconFile}`} alt="" style={{ width: 14, height: 14, objectFit: "contain" }} />
        ) : null}
        {component.name}
        <span style={{ color: "#475569", marginLeft: 2 }}>#{componentId}</span>
      </div>

      {/* Info section */}
      <div style={{ padding: "4px 10px", display: "flex", flexDirection: "column", gap: 2 }}>
        <div style={{ display: "flex", justifyContent: "space-between", fontSize: 11 }}>
          <span style={{ color: "#9ca3af" }}>Status</span>
          <span style={{ background: stateColor, borderRadius: 999, padding: "0 6px" }}>{state}</span>
        </div>
        {Number.isFinite(tempValue) && (
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: 11 }}>
            <span style={{ color: "#9ca3af" }}>Temperature</span>
            <span>{tempValue.toFixed(1)}&deg;C</span>
          </div>
        )}
        {Number.isFinite(powerValue) && (
          <div style={{ display: "flex", justifyContent: "space-between", fontSize: 11 }}>
            <span style={{ color: "#9ca3af" }}>Power</span>
            <span>{powerValue.toFixed(2)}W</span>
          </div>
        )}
        {error && (
          <div style={{ fontSize: 11, color: "#fca5a5", marginTop: 2 }}>
            &#x26A0; {error}
          </div>
        )}
        {effects.map((eff) => (
          <div key={eff} style={{ fontSize: 11, color: eff === "OVERHEAT" ? "#fb923c" : "#60a5fa" }}>
            {eff === "OVERHEAT" ? "\u{1F525}" : "\u{2744}\u{FE0F}"} {eff}
          </div>
        ))}
      </div>

      {/* Actions section */}
      <div style={{ borderTop: "1px solid #334155", marginTop: 2 }}>
        {showStart && (
          <div
            style={menuRowStyle}
            onMouseEnter={(e) => (e.currentTarget.style.background = "#334155")}
            onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
            onClick={() => doAction(() => void setComponentState(rpcClient, frameId, componentId, "active"))}
          >
            <span>&#x25B6;</span><span>Start</span>
          </div>
        )}
        {showStop && (
          <div
            style={menuRowStyle}
            onMouseEnter={(e) => (e.currentTarget.style.background = "#334155")}
            onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
            onClick={() => doAction(() => void setComponentState(rpcClient, frameId, componentId, "inactive"))}
          >
            <span>&#x23F9;</span><span>Stop</span>
          </div>
        )}
        {showRepair && (
          <div
            style={menuRowStyle}
            onMouseEnter={(e) => (e.currentTarget.style.background = "#334155")}
            onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
            onClick={() => doAction(() => void repairComponent(rpcClient, frameId, componentId))}
          >
            <span>&#x1F527;</span><span>Repair</span>
          </div>
        )}
        <div
          style={menuRowStyle}
          onMouseEnter={(e) => (e.currentTarget.style.background = "#334155")}
          onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
          onClick={() => doAction(() => void removeComponent(rpcClient, frameId, componentId))}
        >
          <span style={{ color: "#f87171" }}>&#x2715;</span><span style={{ color: "#f87171" }}>Remove</span>
        </div>
      </div>
    </div>,
    document.body,
  );
}
