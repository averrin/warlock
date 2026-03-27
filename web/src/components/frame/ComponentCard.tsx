import { useRef, useState, type CSSProperties } from "react";
import type { RpcClient } from "../../rpc/client";
import type { ComponentDTO, DataLinkBufferDTO } from "../../rpc/types";
import { Badge, STATE_COLORS, EFFECT_LABELS, SelectField, COMPONENT_SIZES, MATERIALS, componentSizeOptionsAllowed, CardExpandControl, fmtId, copyToClipboard } from "../ui";
import { useGameStore } from "../../stores/game";
import { useWindowLayoutStore } from "../../stores/windowLayout";
import { ComponentControls } from "./ComponentControls";
import { PropulsionMoveControls } from "./PropulsionMoveControls";
import { ComponentAttributes } from "./ComponentAttributes";
import { StoragePanelWithSubscription } from "../storage/StoragePanel";

type Props = {
  component: ComponentDTO;
  frameId: number;
  rpcClient: RpcClient;
};

const DATA_LINK_PREVIEW = 200;

function truncateText(s: string, max: number): string {
  if (s.length <= max) return s;
  return `${s.slice(0, max)}…`;
}

function DataLinkBufferInspector({ buffer }: { buffer: DataLinkBufferDTO }) {
  const raw = buffer.raw_queue ?? [];
  const packets = buffer.packet_queue ?? [];
  const total = raw.length + packets.length;
  const box: CSSProperties = {
    fontSize: 11,
    fontFamily: "ui-monospace, monospace",
    background: "#0f172a",
    border: "1px solid #334155",
    borderRadius: 4,
    padding: 8,
    display: "grid",
    gap: 6,
    color: "#cbd5e1",
  };
  const label: CSSProperties = { color: "#94a3b8", fontSize: 10, textTransform: "uppercase" as const, letterSpacing: "0.04em" };

  return (
    <div style={box}>
      <div style={label}>Data buffer</div>
      <div>
        Counterparts: {buffer.counterpart_id}
        {buffer.counterpart_id_alt >= 0 ? ` / ${buffer.counterpart_id_alt}` : ""}
        {" · "}
        queued {total} / {buffer.max_queue}
      </div>
      {raw.length === 0 && packets.length === 0 ? (
        <div style={{ color: "#64748b" }}>Queues empty</div>
      ) : (
        <>
          {raw.length > 0 && (
            <div style={{ display: "grid", gap: 4 }}>
              <div style={label}>Raw ({raw.length})</div>
              {raw.map((line, i) => (
                <div key={i} style={{ whiteSpace: "pre-wrap", wordBreak: "break-all", color: "#e2e8f0" }}>
                  [{i}] {truncateText(line, DATA_LINK_PREVIEW)}
                </div>
              ))}
            </div>
          )}
          {packets.length > 0 && (
            <div style={{ display: "grid", gap: 4 }}>
              <div style={label}>Packets ({packets.length})</div>
              {packets.map((p, i) => {
                const hdr = Object.entries(p.headers ?? {})
                  .map(([k, v]) => `${k}=${v}`)
                  .join(", ");
                return (
                  <div key={i} style={{ display: "grid", gap: 2 }}>
                    <div style={{ color: "#94a3b8" }}>
                      [{i}] src {p.source} → dst {p.destination}
                      {hdr ? ` · ${hdr}` : ""}
                    </div>
                    <div style={{ whiteSpace: "pre-wrap", wordBreak: "break-all", color: "#e2e8f0" }}>
                      {truncateText(p.body ?? "", DATA_LINK_PREVIEW)}
                    </div>
                  </div>
                );
              })}
            </div>
          )}
        </>
      )}
    </div>
  );
}

/** Extract the string value from a raw code attribute (bare string or AttributeDTO). */
function extractCodeString(val: unknown): string {
  if (typeof val === "string") return val;
  if (typeof val === "object" && val !== null && "base_value" in val) {
    const bv = (val as { base_value: unknown }).base_value;
    return typeof bv === "string" ? bv : "";
  }
  return "";
}

function CopyLocatorMenu({ component }: { component: ComponentDTO }) {
  const [open, setOpen] = useState(false);
  const [copied, setCopied] = useState<string | null>(null);
  const ref = useRef<HTMLDivElement>(null);

  const copy = (text: string, label: string) => {
    copyToClipboard(text);
    setCopied(label);
    setTimeout(() => { setCopied(null); setOpen(false); }, 900);
  };

  const btnStyle: CSSProperties = {
    border: "none",
    background: "transparent",
    color: "#9ca3af",
    cursor: "pointer",
    fontSize: 11,
    padding: "3px 8px",
    textAlign: "left" as const,
    width: "100%",
    whiteSpace: "nowrap" as const,
    borderRadius: 3,
  };

  return (
    <div ref={ref} style={{ position: "relative", flexShrink: 0 }}>
      <button
        type="button"
        title="Copy locator"
        onClick={(e) => { e.stopPropagation(); setOpen((v) => !v); }}
        style={{
          border: "1px solid #334155",
          borderRadius: 4,
          width: 20,
          height: 20,
          background: open ? "#1e293b" : "#020617",
          color: "#6b7280",
          cursor: "pointer",
          fontSize: 11,
          display: "flex",
          alignItems: "center",
          justifyContent: "center",
          padding: 0,
          flexShrink: 0,
        }}
      >
        ⎘
      </button>
      {open && (
        <div
          style={{
            position: "absolute",
            right: 0,
            top: 22,
            background: "#111827",
            border: "1px solid #374151",
            borderRadius: 6,
            zIndex: 100,
            minWidth: 220,
            padding: 4,
            boxShadow: "0 4px 16px #000a",
          }}
          onClick={(e) => e.stopPropagation()}
        >
          {[
            { label: "by id", text: `locator(frame, "#0x${component.id.toString(16).toUpperCase().padStart(2, "0")}")` },
            { label: "by name", text: `locator(frame, ".${component.name}")` },
            { label: "by type", text: `locator(frame, "type:${component.type ?? component.name}")` },
          ].map(({ label, text }) => (
            <button
              key={label}
              type="button"
              style={{ ...btnStyle, color: copied === label ? "#22c55e" : "#9ca3af" }}
              onClick={() => copy(text, label)}
              onMouseEnter={(e) => { (e.target as HTMLElement).style.background = "#1f2937"; }}
              onMouseLeave={(e) => { (e.target as HTMLElement).style.background = "transparent"; }}
            >
              {copied === label ? "✓ " : ""}
              <span style={{ color: "#6b7280", fontSize: 10 }}>{label}: </span>
              <span style={{ fontFamily: "ui-monospace, monospace", fontSize: 10, color: "#e5e7eb" }}>{text}</span>
            </button>
          ))}
        </div>
      )}
    </div>
  );
}

export function ComponentCard({ component, frameId, rpcClient }: Props) {
  const [expanded, setExpanded] = useState(false);
  const setComponentSize = useGameStore((s) => s.setComponentSize);
  const setComponentMaterial = useGameStore((s) => s.setComponentMaterial);
  const frameForSlots = useGameStore((s) => s.frames.find((f) => f.id === frameId));
  const componentSizeOptions = componentSizeOptionsAllowed(
    frameForSlots?.component_slots,
    component.size ?? "S",
    COMPONENT_SIZES,
  );
  const openComponentCodeEditor = useWindowLayoutStore((s) => s.openComponentCodeEditor);

  const stateColor = STATE_COLORS[component.state] ?? "#1f2937";
  const effects = (component.effects ?? []).map((e) => EFFECT_LABELS[e] ?? e);
  const iconFile = (component.metadata?.icon || component.icon || "").trim();
  const glyph = (component.name || "?")[0]!.toUpperCase();

  const isFrozen = (component.effects ?? []).includes("FREEZE");
  const isOverheat = (component.effects ?? []).includes("OVERHEAT");
  let borderColor = "#334155";
  if (isFrozen) borderColor = "#3b82f6";
  else if (isOverheat) borderColor = "#f97316";

  let codeAttrKey: string | null = null;
  let codeAttrRaw: unknown;
  for (const [k, v] of Object.entries(component.attributes ?? {})) {
    const dto = v as { inspector?: { widget?: string } };
    if (dto && typeof dto === "object" && dto.inspector?.widget === "code") {
      codeAttrKey = k;
      codeAttrRaw = v;
      break;
    }
  }
  const hasCodeAttr = codeAttrKey !== null;
  const isPropulsion = component.name === "Propulsion" || component.type === "Propulsion";

  const openLuaDef = (e: React.MouseEvent) => {
    e.stopPropagation();
    openComponentCodeEditor(
      { mode: "source", sourceName: component.name.toLowerCase() },
      `λ ${component.name}`,
    );
  };

  const openCodeAttr = () => {
    if (codeAttrKey === null) return;
    openComponentCodeEditor(
      {
        mode: "attribute",
        frameId,
        componentId: component.id,
        attrKey: codeAttrKey,
        initialCode: extractCodeString(codeAttrRaw),
      },
      `📝 ${component.name} #${component.id} — code`,
    );
  };

  return (
    <div style={{ border: "1px solid #334155", borderRadius: 6, padding: "6px 8px", display: "grid", gap: 6 }}>
      {/* Header: icon | name id effects badge | flex-1 | λ | action buttons | expand control */}
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
        <Badge label={fmtId(component.id)} variant="id" title={`ID ${component.id} — click to copy`} onClick={() => copyToClipboard(`#${component.id}`)} />
        {effects.map((eff, i) => (
          <span key={i} style={{ fontSize: 11 }}>{eff}</span>
        ))}
        <Badge label={component.state} variant="state" />

        <div style={{ flex: 1 }} />

        {/* Lua def button — always shown */}
        <button
          type="button"
          title={`Open ${component.name} Lua definition`}
          onClick={openLuaDef}
          style={{
            border: "1px solid #334155",
            borderRadius: 4,
            width: 20,
            height: 20,
            background: "#020617",
            color: "#6b7280",
            cursor: "pointer",
            fontSize: 11,
            display: "flex",
            alignItems: "center",
            justifyContent: "center",
            padding: 0,
            flexShrink: 0,
          }}
        >
          λ
        </button>

        <CopyLocatorMenu component={component} />
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
              options={componentSizeOptions}
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
          {isPropulsion && (
            <PropulsionMoveControls frameId={frameId} componentId={component.id} rpcClient={rpcClient} />
          )}
          {component.type === "Data Connector" && component.data_link_buffer != null && (
            <DataLinkBufferInspector buffer={component.data_link_buffer} />
          )}
          {/* Code attribute editor button */}
          {hasCodeAttr && (
            <button
              type="button"
              onClick={openCodeAttr}
              style={{
                fontSize: 11,
                padding: "4px 10px",
                background: "#0f1f35",
                border: "1px solid #1d4ed8",
                borderRadius: 4,
                color: "#60a5fa",
                cursor: "pointer",
                textAlign: "left",
                alignSelf: "flex-start",
              }}
            >
              📝 Edit Instance Code
            </button>
          )}
          {component.storage && (
            <StoragePanelWithSubscription frameId={frameId} componentId={component.id} mode="full" rpcClient={rpcClient} />
          )}
        </>
      )}
    </div>
  );
}
