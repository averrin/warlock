import type { CSSProperties } from "react";
import type { RpcClient } from "../../rpc/client";
import type { ComponentDTO } from "../../rpc/types";
import { useGameStore } from "../../stores/game";
import { NumberField, SelectField, TextField } from "../ui";

const CONVEYOR_CONNECTOR_NAME = "Conveyor Connector";
const CONVEYOR_MODE_OPTIONS = ["SEND", "RECEIVE"] as const;

const targetSelectStyle: CSSProperties = {
  fontSize: 11,
  borderRadius: 3,
  border: "1px solid #374151",
  background: "#0b1220",
  color: "#e5e7eb",
  padding: "1px 4px",
};

type AttributeValue = string | number | boolean | unknown;

// Full AttributeDTO shape from the backend
interface AttributeDTO {
  title?: string;
  type?: string;
  base_value: string | number | boolean;
  final_value?: string | number | boolean;
  modifiers?: string[];
}

function isAttributeDTO(v: unknown): v is AttributeDTO {
  return (
    typeof v === "object" &&
    v !== null &&
    "base_value" in v
  );
}

// Resolve the effective type string: from DTO type field or inferred from value
function resolveType(val: unknown): "float" | "int" | "bool" | "string" {
  if (isAttributeDTO(val)) {
    const t = (val.type ?? "").toUpperCase();
    if (t === "FLOAT") return "float";
    if (t === "INT") return "int";
    if (t === "BOOL") return "bool";
    if (t === "STRING") return "string";
    // Fallback to inferred type from base_value
    return resolveType(val.base_value);
  }
  if (typeof val === "number") return Number.isInteger(val) ? "int" : "float";
  if (typeof val === "boolean") return "bool";
  return "string";
}

type SingleAttributeProps = {
  attrKey: string;
  val: AttributeValue;
  frameId: number;
  componentId: number;
  componentName: string;
  frameComponents: ComponentDTO[];
  rpcClient: RpcClient;
  updateAttribute: (
    client: RpcClient,
    frameId: number,
    componentId: number,
    key: string,
    value: string | number | boolean,
  ) => Promise<void>;
};

function SingleAttribute({
  attrKey,
  val,
  frameId,
  componentId,
  componentName,
  frameComponents,
  rpcClient,
  updateAttribute,
}: SingleAttributeProps) {
  const isDTO = isAttributeDTO(val);
  const rawValue = isDTO ? val.base_value : (val as string | number | boolean);
  const finalValue = isDTO ? val.final_value : undefined;
  const modifiers = isDTO ? (val.modifiers ?? []) : [];
  const label = isDTO && val.title ? val.title : attrKey;
  const type = resolveType(val);

  const hasModifierEffect =
    finalValue !== undefined && finalValue !== rawValue;

  const onChange = (newVal: string | number | boolean) =>
    void updateAttribute(rpcClient, frameId, componentId, attrKey, newVal);

  const isConveyorModeSelect =
    componentName === CONVEYOR_CONNECTOR_NAME && attrKey === "mode" && type === "string";
  const modeStr =
    typeof rawValue === "string" && rawValue.length > 0 ? rawValue : "SEND";
  const modeOptions = !(CONVEYOR_MODE_OPTIONS as readonly string[]).includes(modeStr)
    ? [modeStr, ...CONVEYOR_MODE_OPTIONS]
    : [...CONVEYOR_MODE_OPTIONS];

  const isTargetComponentSelect = attrKey === "target" && type === "int";
  const targetIdRaw = typeof rawValue === "number" ? rawValue : Number(rawValue);
  const targetId = Number.isFinite(targetIdRaw) ? Math.trunc(targetIdRaw) : -1;
  const targetCandidates = frameComponents.filter((c) => c.id !== componentId);
  const targetIds = new Set(targetCandidates.map((c) => c.id));
  const orphanTarget = targetId >= 0 && !targetIds.has(targetId);

  return (
    <div style={{ display: "grid", gap: 2 }}>
      <div style={{ display: "flex", alignItems: "center", gap: 6 }}>
        {isConveyorModeSelect ? (
          <SelectField
            label={label}
            value={modeStr}
            options={modeOptions}
            onChange={(v) => onChange(v)}
          />
        ) : isTargetComponentSelect ? (
          <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
            <span style={{ minWidth: 90, fontSize: 11, color: "#9ca3af" }}>{label}:</span>
            <select
              value={targetId < 0 ? "" : String(targetId)}
              onChange={(e) => {
                const v = e.target.value;
                onChange(v === "" ? -1 : Number(v));
              }}
              style={targetSelectStyle}
            >
              <option value="">None</option>
              {orphanTarget && (
                <option value={String(targetId)}>#{targetId} (unavailable)</option>
              )}
              {targetCandidates.map((c) => (
                <option key={c.id} value={String(c.id)}>
                  {c.name} (#{c.id})
                </option>
              ))}
            </select>
          </div>
        ) : type === "bool" ? (
          <div style={{ display: "flex", alignItems: "center", gap: 8, padding: "1px 0" }}>
            <span style={{ minWidth: 90, fontSize: 11, color: "#9ca3af" }}>{label}:</span>
            <input
              type="checkbox"
              checked={rawValue as boolean}
              onChange={(e) => onChange(e.target.checked)}
            />
          </div>
        ) : type === "float" || type === "int" ? (
          <NumberField
            label={label}
            value={rawValue as number}
            onChange={onChange}
          />
        ) : (
          <TextField
            label={label}
            value={rawValue as string}
            onChange={onChange}
          />
        )}

        {/* Final value indicator when modifiers alter the base */}
        {hasModifierEffect && (
          <span
            title="Final value after modifiers"
            style={{ fontSize: 10, color: "#60a5fa", whiteSpace: "nowrap" }}
          >
            → {String(finalValue)}
          </span>
        )}
      </div>

      {/* Modifiers list */}
      {modifiers.length > 0 && (
        <div style={{ display: "flex", gap: 4, flexWrap: "wrap", paddingLeft: 94 }}>
          {modifiers.map((mod, i) => (
            <span
              key={i}
              style={{
                fontSize: 10,
                color: "#fbbf24",
                background: "#1c1500",
                border: "1px solid #3b2a00",
                borderRadius: 3,
                padding: "0 4px",
              }}
            >
              {mod}
            </span>
          ))}
        </div>
      )}
    </div>
  );
}

type Props = {
  frameId: number;
  componentId: number;
  componentName: string;
  attributes: Record<string, AttributeValue> | undefined;
  rpcClient: RpcClient;
};

export function ComponentAttributes({
  frameId,
  componentId,
  componentName,
  attributes,
  rpcClient,
}: Props) {
  const updateComponentAttribute = useGameStore((s) => s.updateComponentAttribute);
  const frameComponents = useGameStore(
    (s) => s.frames.find((f) => f.id === frameId)?.components ?? [],
  );

  if (!attributes) return null;

  const entries = Object.entries(attributes).filter(([key]) => key !== "code");

  if (entries.length === 0) return null;

  return (
    <div style={{ display: "grid", gap: 4 }}>
      <div style={{ fontSize: 11, fontWeight: 600, color: "#9ca3af" }}>Attributes</div>
      {entries.map(([key, val]) => (
        <SingleAttribute
          key={key}
          attrKey={key}
          val={val}
          frameId={frameId}
          componentId={componentId}
          componentName={componentName}
          frameComponents={frameComponents}
          rpcClient={rpcClient}
          updateAttribute={updateComponentAttribute}
        />
      ))}
    </div>
  );
}
