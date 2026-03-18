import { useRef, useState } from "react";
import { createPortal } from "react-dom";
import type { RpcClient } from "../../rpc/client";
import type { ComponentDTO } from "../../rpc/types";
import { useGameStore } from "../../stores/game";
import { STATE_COLORS, EFFECT_LABELS } from "../ui";

type Props = {
  frameId: number;
  components: ComponentDTO[];
  rpcClient: RpcClient;
  availableComponents?: string[];
  netAvailable?: number | null;
  onComponentContextMenu?: (e: React.MouseEvent, componentId: number) => void;
};

function isComponentActive(state: string | undefined): boolean {
  const s = (state ?? "").toUpperCase();
  return s === "ACTIVE" || s === "ACTIVATING";
}

function getComponentBadgeColor(state: string | undefined): string {
  const s = state ?? "DEACTIVATED";
  return STATE_COLORS[s] ?? STATE_COLORS[s.toUpperCase()] ?? "#1f2937";
}

export function ComponentStrip({ frameId, components, rpcClient, availableComponents, netAvailable: _netAvailable, onComponentContextMenu }: Props) {
  const setComponentState = useGameStore((s) => s.setComponentState);
  const addComponent = useGameStore((s) => s.addComponent);
  const [menuOpen, setMenuOpen] = useState(false);
  const addBtnRef = useRef<HTMLButtonElement>(null);

  return (
    <div style={{ display: "flex", gap: 6, flexWrap: "wrap", alignItems: "center" }}>
      {components.length === 0 ? (
        <div style={{ fontSize: 12, color: "#9ca3af" }}>No components</div>
      ) : (
        components.map((component) => {
          const isActive = isComponentActive(component.state);
          const nextState = isActive ? "inactive" : "active";
          const effects = (component.effects ?? []).map((e) => EFFECT_LABELS[e] ?? e).join(" ");
          const iconFile = (component.metadata?.icon || component.icon || "").trim();
          const label = (iconFile || component.name || "?").trim();
          const glyph = label.length > 0 ? label[0]!.toUpperCase() : "?";
          const isFrozen = (component.effects ?? []).includes("FREEZE");
          const isOverheat = (component.effects ?? []).includes("OVERHEAT");

          let borderColor = "#334155";
          if (isFrozen) borderColor = "#3b82f6";
          else if (isOverheat) borderColor = "#f97316";

          const title = `${component.name} (#${component.id})\nstate: ${component.state}${effects ? `\neffects: ${effects}` : ""}`;

          return (
            <button
              key={component.id}
              type="button"
              onClick={() => void setComponentState(rpcClient, frameId, component.id, nextState)}
              onContextMenu={(e) => {
                e.preventDefault();
                onComponentContextMenu?.(e, component.id);
              }}
              title={title}
              style={{
                border: `1px solid ${borderColor}`,
                borderRadius: 8,
                padding: 0,
                width: 28,
                height: 28,
                background: getComponentBadgeColor(component.state),
                color: "#e2e8f0",
                cursor: "pointer",
                fontSize: 11,
                display: "flex",
                alignItems: "center",
                justifyContent: "center",
              }}
            >
              {iconFile ? (
                <img src={`/icons/${iconFile}`} alt={component.name} style={{ width: 20, height: 20, objectFit: "contain" }} />
              ) : (
                <span style={{ fontWeight: 700 }}>{glyph}</span>
              )}
            </button>
          );
        })
      )}
      {availableComponents && availableComponents.length > 0 && (
        <>
          <button
            ref={addBtnRef}
            type="button"
            onClick={() => setMenuOpen((open) => !open)}
            title="Add component"
            style={{
              border: "1px solid #334155",
              borderRadius: 8,
              padding: 0,
              width: 28,
              height: 28,
              background: "#020617",
              color: "#e5e7eb",
              cursor: "pointer",
              fontSize: 16,
              display: "flex",
              alignItems: "center",
              justifyContent: "center",
            }}
          >
            +
          </button>
          {menuOpen && addBtnRef.current && createPortal(
            (() => {
              const rect = addBtnRef.current!.getBoundingClientRect();
              return (
                <div
                  style={{
                    position: "fixed",
                    top: rect.bottom + 4,
                    left: rect.left,
                    zIndex: 9999,
                    maxHeight: 220,
                    overflowY: "auto",
                    background: "#020617",
                    border: "1px solid #1f2937",
                    borderRadius: 6,
                    boxShadow: "0 8px 24px rgba(0,0,0,0.7)",
                  }}
                  onMouseLeave={() => setMenuOpen(false)}
                >
                  {availableComponents.map((name) => (
                    <button
                      key={name}
                      type="button"
                      onClick={() => {
                        void addComponent(rpcClient, frameId, name);
                        setMenuOpen(false);
                      }}
                      style={{
                        display: "block",
                        textAlign: "left",
                        padding: "3px 10px",
                        border: "none",
                        background: "transparent",
                        color: "#e5e7eb",
                        fontSize: 11,
                        cursor: "pointer",
                        whiteSpace: "nowrap",
                      }}
                      onMouseEnter={(e) => (e.currentTarget.style.background = "#1e293b")}
                      onMouseLeave={(e) => (e.currentTarget.style.background = "transparent")}
                    >
                      {name}
                    </button>
                  ))}
                </div>
              );
            })(),
            document.body
          )}
        </>
      )}
    </div>
  );
}
